// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"

namespace speech {

// This is Chrome's implementation of the SpeechRecognitionManagerDelegate
// interface.
class ChromeSpeechRecognitionManagerDelegate
    : public content::SpeechRecognitionManagerDelegate,
      public content::SpeechRecognitionEventListener {
 public:
  ChromeSpeechRecognitionManagerDelegate();

  ChromeSpeechRecognitionManagerDelegate(
      const ChromeSpeechRecognitionManagerDelegate&) = delete;
  ChromeSpeechRecognitionManagerDelegate& operator=(
      const ChromeSpeechRecognitionManagerDelegate&) = delete;

  ~ChromeSpeechRecognitionManagerDelegate() override;

 protected:
  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& result)
      override;
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  content::SpeechRecognitionEventListener* GetEventListener() override;
#if !BUILDFLAG(IS_ANDROID)
  // This will bind to the Speech Recognition Service if available.
  // On LaCros, it will forward to Ash. On other platforms (Ash, Desktop), it
  // will bind to appropriate Speech Recognition Service when enabled.
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  // Checks for mojom::ViewType::kTabContents host in the UI thread and notifies
  // back the result in the IO thread through |callback|.
  static void CheckRenderFrameType(
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_frame_id);
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
