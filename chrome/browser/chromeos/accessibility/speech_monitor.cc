// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/speech_monitor.h"

#include "content/public/browser/tts_controller.h"

namespace chromeos {

namespace {
const char kChromeVoxEnabledMessage[] = "ChromeVox spoken feedback is ready";
const char kChromeVoxAlertMessage[] = "Alert";
const char kChromeVoxUpdate1[] = "chrome vox Updated Press chrome vox o,";
const char kChromeVoxUpdate2[] = "n to learn more about chrome vox Next.";
}  // namespace

SpeechMonitor::SpeechMonitor() {
  content::TtsController::GetInstance()->SetTtsPlatform(this);
}

SpeechMonitor::~SpeechMonitor() {
  content::TtsController::GetInstance()->SetTtsPlatform(
      content::TtsPlatform::GetInstance());
}

std::string SpeechMonitor::GetNextUtterance() {
  return GetNextUtteranceWithLanguage().text;
}

SpeechMonitorUtterance SpeechMonitor::GetNextUtteranceWithLanguage() {
  if (utterance_queue_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_.reset();
  }
  SpeechMonitorUtterance result = utterance_queue_.front();
  utterance_queue_.pop_front();
  return result;
}

bool SpeechMonitor::SkipChromeVoxEnabledMessage() {
  return SkipChromeVoxMessage(kChromeVoxEnabledMessage);
}

bool SpeechMonitor::DidStop() {
  return did_stop_;
}

void SpeechMonitor::BlockUntilStop() {
  if (!did_stop_) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_.reset();
  }
}

bool SpeechMonitor::SkipChromeVoxMessage(const std::string& message) {
  while (true) {
    if (utterance_queue_.empty()) {
      loop_runner_ = new content::MessageLoopRunner();
      loop_runner_->Run();
      loop_runner_.reset();
    }
    SpeechMonitorUtterance result = utterance_queue_.front();
    utterance_queue_.pop_front();
    if (result.text == message)
      return true;
  }
  return false;
}

bool SpeechMonitor::PlatformImplAvailable() {
  return true;
}

void SpeechMonitor::Speak(int utterance_id,
                          const std::string& utterance,
                          const std::string& lang,
                          const content::VoiceData& voice,
                          const content::UtteranceContinuousParameters& params,
                          base::OnceCallback<void(bool)> on_speak_finished) {
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id, content::TTS_EVENT_END, static_cast<int>(utterance.size()),
      0, std::string());
  std::move(on_speak_finished).Run(true);
  delay_for_last_utterance_MS_ = CalculateUtteranceDelayMS();
  time_of_last_utterance_ = std::chrono::steady_clock::now();
}

bool SpeechMonitor::StopSpeaking() {
  did_stop_ = true;
  return true;
}

bool SpeechMonitor::IsSpeaking() {
  return false;
}

void SpeechMonitor::GetVoices(std::vector<content::VoiceData>* out_voices) {
  out_voices->push_back(content::VoiceData());
  content::VoiceData& voice = out_voices->back();
  voice.native = true;
  voice.name = "SpeechMonitor";
  voice.events.insert(content::TTS_EVENT_END);
}

void SpeechMonitor::WillSpeakUtteranceWithVoice(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice_data) {
  // Blacklist some phrases.
  // Filter out empty utterances which can be used to trigger a start event from
  // tts as an earcon sync.
  if (utterance->GetText() == "" ||
      utterance->GetText() == kChromeVoxAlertMessage ||
      utterance->GetText() == kChromeVoxUpdate1 ||
      utterance->GetText() == kChromeVoxUpdate2)
    return;

  VLOG(0) << "Speaking " << utterance->GetText();
  utterance_queue_.emplace_back(utterance->GetText(), utterance->GetLang());
  if (loop_runner_.get())
    loop_runner_->Quit();
}

bool SpeechMonitor::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  return false;
}

std::string SpeechMonitor::GetError() {
  return error_;
}

void SpeechMonitor::ClearError() {
  error_ = std::string();
}

void SpeechMonitor::SetError(const std::string& error) {
  error_ = error;
}

double SpeechMonitor::CalculateUtteranceDelayMS() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  std::chrono::duration<double> time_span =
      std::chrono::duration_cast<std::chrono::duration<double>>(
          now - time_of_last_utterance_);
  return time_span.count() * 1000;
}

double SpeechMonitor::GetDelayForLastUtteranceMS() {
  return delay_for_last_utterance_MS_;
}

}  // namespace chromeos
