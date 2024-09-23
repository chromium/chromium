// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/network_speech_recognizer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Invalid speech session.
static const int kInvalidSessionId = -1;

// Speech recognizer listener. This is separate from SpeechRecognizer because
// the speech recognition engine must function from the IO thread. Because of
// this, the lifecycle of this class must be decoupled from the lifecycle of
// SpeechRecognizer. To avoid circular references, this class has no reference
// to SpeechRecognizer. Instead, it has a reference to the
// SpeechRecognizerDelegate via a weak pointer that is only ever referenced from
// the UI thread.
class NetworkSpeechRecognizer::EventListener
    : public base::RefCountedThreadSafe<
          NetworkSpeechRecognizer::EventListener,
          content::BrowserThread::DeleteOnIOThread>,
      public content::SpeechRecognitionEventListener {
 public:
  EventListener(const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
                std::unique_ptr<network::PendingSharedURLLoaderFactory>
                    pending_shared_url_loader_factory,
                const std::string& accept_language,
                const std::string& locale);

  EventListener(const EventListener&) = delete;
  EventListener& operator=(const EventListener&) = delete;

  void StartOnIOThread(
      const std::string& auth_scope,
      const std::string& auth_token,
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble);
  void StopOnIOThread();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<NetworkSpeechRecognizer::EventListener>;
  ~EventListener() override;

  void NotifyRecognitionStateChanged(SpeechRecognizerStatus new_state);

  // Overridden from content::SpeechRecognitionEventListener:
  // These are always called on the IO thread.
  void OnRecognitionStart(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results)
      override;
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;
  void OnAudioStart(int session_id) override;
  void OnAudioEnd(int session_id) override;

  // Only dereferenced from the UI thread, but copied on IO thread.
  base::WeakPtr<SpeechRecognizerDelegate> delegate_;

  // All remaining members only accessed from the IO thread.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_shared_url_loader_factory_;
  // Initialized from |pending_shared_url_loader_factory_| on first use.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  const std::string accept_language_;
  std::string locale_;
  int session_;
  std::u16string last_result_str_;

  base::WeakPtrFactory<EventListener> weak_factory_{this};
};

NetworkSpeechRecognizer::EventListener::EventListener(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory,
    const std::string& accept_language,
    const std::string& locale)
    : delegate_(delegate),
      pending_shared_url_loader_factory_(
          std::move(pending_shared_url_loader_factory)),
      accept_language_(accept_language),
      locale_(locale),
      session_(kInvalidSessionId) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_READY);
}

NetworkSpeechRecognizer::EventListener::~EventListener() {
  // No more callbacks when we are deleting.
  delegate_.reset();
  if (session_ != kInvalidSessionId) {
    // Ensure the session is aborted.
    int session = session_;
    session_ = kInvalidSessionId;
    content::SpeechRecognitionManager::GetInstance()->AbortSession(session);
  }
}

void NetworkSpeechRecognizer::EventListener::StartOnIOThread(
    const std::string& auth_scope,
    const std::string& auth_token,
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (session_ != kInvalidSessionId)
    StopOnIOThread();

  // Don't filter profanities. NetworkSpeechRecognizer is currently used by
  // Dictation which does not want to filter user input. If this needs to be
  // changed for other clients in the future, whether to filter should be passed
  // as a parameter to the speech recognizer instead of changed here.
  bool filter_profanities = false;
  content::SpeechRecognitionSessionConfig config;
  config.language = locale_;
  config.continuous = true;
  config.interim_results = true;
  config.max_hypotheses = 1;
  config.filter_profanities = filter_profanities;
  config.accept_language = accept_language_;
  if (!shared_url_loader_factory_) {
    DCHECK(pending_shared_url_loader_factory_);
    shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_shared_url_loader_factory_));
  }
  config.shared_url_loader_factory = shared_url_loader_factory_;
  config.event_listener = weak_factory_.GetWeakPtr();
  // kInvalidUniqueID is not a valid render process, so the speech permission
  // check allows the request through.
  config.initial_context.render_process_id =
      content::ChildProcessHost::kInvalidUniqueID;
  config.auth_scope = auth_scope;
  config.auth_token = auth_token;
  config.preamble = preamble;

  auto* speech_instance = content::SpeechRecognitionManager::GetInstance();
  session_ = speech_instance->CreateSession(config);
  speech_instance->StartSession(session_);
}

void NetworkSpeechRecognizer::EventListener::StopOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (session_ == kInvalidSessionId)
    return;

  // Prevent recursion.
  int session = session_;
  session_ = kInvalidSessionId;
  content::SpeechRecognitionManager::GetInstance()->StopAudioCaptureForSession(
      session);
  // Since we no longer have access to this session ID, end the session
  // associated with it.
  content::SpeechRecognitionManager::GetInstance()->AbortSession(session);
  weak_factory_.InvalidateWeakPtrs();
}

void NetworkSpeechRecognizer::EventListener::NotifyRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognizerDelegate::OnSpeechRecognitionStateChanged,
                     delegate_, new_state));
}

void NetworkSpeechRecognizer::EventListener::OnRecognitionStart(
    int session_id) {
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING);
}

void NetworkSpeechRecognizer::EventListener::OnRecognitionEnd(int session_id) {
  StopOnIOThread();
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_READY);
}

void NetworkSpeechRecognizer::EventListener::OnRecognitionResults(
    int session_id,
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
  std::u16string result_str;
  size_t final_count = 0;
  // The number of results with |is_provisional| false. If |final_count| ==
  // results.size(), then all results are non-provisional and the recognition is
  // complete.
  for (const auto& result : results) {
    if (!result->is_provisional)
      final_count++;
    result_str += result->hypotheses[0]->utterance;
  }
  // media::mojom::WebSpeechRecognitionResult doesn't have word offsets.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognizerDelegate::OnSpeechResult, delegate_,
                     result_str, final_count == results.size(),
                     /* full_result = */ std::nullopt));

  last_result_str_ = result_str;
}

void NetworkSpeechRecognizer::EventListener::OnRecognitionError(
    int session_id,
    const media::mojom::SpeechRecognitionError& error) {
  StopOnIOThread();
  if (error.code == media::mojom::SpeechRecognitionErrorCode::kNetwork) {
    NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_ERROR);
  }
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_READY);
}

void NetworkSpeechRecognizer::EventListener::OnSoundStart(int session_id) {
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_IN_SPEECH);
}

void NetworkSpeechRecognizer::EventListener::OnSoundEnd(int session_id) {
  StopOnIOThread();
  NotifyRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING);
}

void NetworkSpeechRecognizer::EventListener::OnAudioLevelsChange(
    int session_id,
    float volume,
    float noise_volume) {
  DCHECK_LE(0.0, volume);
  DCHECK_GE(1.0, volume);
  DCHECK_LE(0.0, noise_volume);
  DCHECK_GE(1.0, noise_volume);
  volume = std::max(0.0f, volume - noise_volume);
  // Both |volume| and |noise_volume| are defined to be in the range [0.0, 1.0].
  // See: content/public/browser/speech_recognition_event_listener.h
  int16_t sound_level = static_cast<int16_t>(INT16_MAX * volume);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognizerDelegate::OnSpeechSoundLevelChanged,
                     delegate_, sound_level));
}

void NetworkSpeechRecognizer::EventListener::OnAudioStart(int session_id) {}

void NetworkSpeechRecognizer::EventListener::OnAudioEnd(int session_id) {}

NetworkSpeechRecognizer::NetworkSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory,
    const std::string& accept_language,
    const std::string& locale)
    : SpeechRecognizer(delegate),
      speech_event_listener_(
          new EventListener(delegate,
                            std::move(pending_shared_url_loader_factory),
                            accept_language,
                            locale)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

NetworkSpeechRecognizer::~NetworkSpeechRecognizer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Reset the delegate before calling Stop() to avoid any additional callbacks.
  delegate().reset();
  Stop();
}

void NetworkSpeechRecognizer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkSpeechRecognizer::EventListener::StartOnIOThread,
                     speech_event_listener_, std::string() /* auth scope*/,
                     std::string() /* auth_token */, /* preamble */ nullptr));
}

void NetworkSpeechRecognizer::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkSpeechRecognizer::EventListener::StopOnIOThread,
                     speech_event_listener_));
}
