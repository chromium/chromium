// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/conversation_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/ttc_mes_client.h"

namespace ttc {

namespace {
Conversation::FactoryCallback* GetFactory() {
  static base::NoDestructor<Conversation::FactoryCallback> factory;
  return factory.get();
}
}  // namespace

// static
std::unique_ptr<Conversation> Conversation::Create(Profile* profile) {
  if (auto* factory = GetFactory(); *factory) {
    return factory->Run(profile);
  }
  return std::make_unique<ConversationImpl>(profile);
}

// static
void Conversation::SetFactoryForTesting(FactoryCallback factory) {
  *GetFactory() = std::move(factory);
}

ConversationImpl::ConversationImpl(Profile* profile)
    : backend_(std::make_unique<TtcMesClient>(profile, this)),
      audio_controller_(std::make_unique<AudioController>(
          AudioController::GetDefaultAudioStreamFactoryBinder())) {}

ConversationImpl::ConversationImpl(
    std::unique_ptr<TtcBackend> backend,
    std::unique_ptr<AudioController> audio_controller)
    : backend_(std::move(backend)),
      audio_controller_(std::move(audio_controller)) {
  if (backend_) {
    backend_->set_observer(this);
  }
}

ConversationImpl::~ConversationImpl() {
  Stop();
}

void ConversationImpl::AddObserver(Conversation::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConversationImpl::RemoveObserver(Conversation::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ConversationImpl::Start() {
  if (audio_controller_) {
    audio_capture_subscription_ =
        audio_controller_->AddAudioCaptureListener(base::BindRepeating(
            &ConversationImpl::OnCapturedAudio, base::Unretained(this)));
    playback_completion_subscription_ =
        audio_controller_->AddPlaybackCompletionListener(base::BindRepeating(
            &ConversationImpl::OnPlaybackCompleted, base::Unretained(this)));
    audio_controller_->StartCapture();
  }

  if (backend_) {
    backend_->Connect();
  }
}

void ConversationImpl::Stop() {
  audio_capture_subscription_ = {};
  playback_completion_subscription_ = {};

  if (audio_controller_) {
    audio_controller_->StopCapture();
    audio_controller_->StopPlayback();
  }

  if (backend_ && backend_->is_connected()) {
    backend_->Close();
  }
}

bool ConversationImpl::is_connected() const {
  return backend_ && backend_->is_connected();
}

void ConversationImpl::SendTextInput(const std::string& text) {
  if (backend_) {
    backend_->SendTextInput(text);
  }
}

void ConversationImpl::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  if (backend_) {
    backend_->SendContextUpdate(url, title, apc);
  }
}

void ConversationImpl::SendToolSetUpdate(
    const std::vector<ToolDefinition>& tools) {
  if (backend_) {
    backend_->SendToolSetUpdate(tools);
  }
}

std::vector<uint8_t> Downsample48kHzTo16kHz(
    base::span<const uint8_t> pcm_data) {
  if (pcm_data.size() < sizeof(int16_t) * 3 ||
      pcm_data.size() % sizeof(int16_t) != 0) {
    return {};
  }
  size_t num_samples = pcm_data.size() / sizeof(int16_t);
  size_t out_samples = num_samples / 3;
  std::vector<int16_t> downsampled_samples(out_samples);
  for (size_t i = 0; i < out_samples; ++i) {
    int16_t s0 = base::I16FromLittleEndian(
        pcm_data.subspan((i * 3) * sizeof(int16_t)).first<2>());
    int16_t s1 = base::I16FromLittleEndian(
        pcm_data.subspan((i * 3 + 1) * sizeof(int16_t)).first<2>());
    int16_t s2 = base::I16FromLittleEndian(
        pcm_data.subspan((i * 3 + 2) * sizeof(int16_t)).first<2>());
    int32_t sum = static_cast<int32_t>(s0) + static_cast<int32_t>(s1) +
                  static_cast<int32_t>(s2);
    downsampled_samples[i] = static_cast<int16_t>(sum / 3);
  }
  auto out_bytes = base::as_byte_span(downsampled_samples);
  return std::vector<uint8_t>(out_bytes.begin(), out_bytes.end());
}

void ConversationImpl::OnCapturedAudio(const std::vector<uint8_t>& pcm_data,
                                       const media::AudioParameters& params) {
  if (!backend_) {
    return;
  }
  if (params.sample_rate() == 48000 && pcm_data.size() >= sizeof(int16_t) * 3) {
    backend_->SendAudioChunk(Downsample48kHzTo16kHz(pcm_data));
    return;
  }
  backend_->SendAudioChunk(pcm_data);
}

void ConversationImpl::OnPlaybackCompleted(int64_t sequence_number) {
  if (backend_) {
    backend_->ReportPlaybackStatus(sequence_number);
  }
}

void ConversationImpl::OnStreamingStateChanged(
    bool connected,
    const std::string& session_id,
    const std::string& error_message) {
  for (auto& observer : observers_) {
    observer.OnConversationStateChanged(connected, session_id, error_message);
  }
}

void ConversationImpl::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  for (auto& observer : observers_) {
    observer.OnTranscriptions(input_transcription, output_transcription);
  }
}

void ConversationImpl::OnAudioOutput(const std::vector<uint8_t>& audio_data,
                                     int64_t sequence_number) {
  if (audio_controller_) {
    audio_controller_->PlayAudio(audio_data, sequence_number);
  }
}

void ConversationImpl::OnGenerationStateChanged(bool started,
                                                bool completed,
                                                bool interrupted) {
  if (interrupted && audio_controller_) {
    audio_controller_->ClearPlaybackQueue();
  }
  for (auto& observer : observers_) {
    observer.OnGenerationStateChanged(started, completed, interrupted);
  }
}

void ConversationImpl::OnToolCall(const std::string& name,
                                  base::DictValue arguments,
                                  ToolResponseCallback response_callback) {
  if (!observers_.empty()) {
    observers_.begin()->OnToolCall(name, std::move(arguments),
                                   std::move(response_callback));
  }
}

}  // namespace ttc
