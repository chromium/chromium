// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/speech_monitor.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/test/test_utils.h"

namespace ash {
namespace test {
namespace {

constexpr int kPrintExpectationDelayMs = 3000;

}  // namespace

SpeechMonitor::Expectation::Expectation(const std::string& text)
    : text_(text) {}
SpeechMonitor::Expectation::~Expectation() = default;
SpeechMonitor::Expectation::Expectation(const Expectation&) = default;

base::circular_deque<SpeechMonitorUtterance>::const_iterator
SpeechMonitor::Expectation::Matches(
    const base::circular_deque<SpeechMonitorUtterance>& queue) const {
  std::vector<std::string> all_text;
  for (auto it = queue.begin(); it != queue.end(); it++) {
    if (base::Contains(disallowed_text_, it->text)) {
      break;
    }

    all_text.push_back(it->text);
    std::string joined_all_text = base::JoinString(all_text, " ");
    bool text_match = as_pattern_
                          ? (base::MatchPattern(it->text, text_) ||
                             base::MatchPattern(joined_all_text, "*" + text_))
                          : (it->text == text_ ||
                             joined_all_text.find(text_) != std::string::npos);
    if (!text_match) {
      continue;
    }

    bool locale_match = !locale_ || it->lang == locale_;
    if (!locale_match) {
      continue;
    }

    return it;
  }
  return queue.end();
}

std::string SpeechMonitor::Expectation::ToString() const {
  std::string ret = "\"" + text_ + "\"";
  std::string options = OptionsToString();
  if (!options.empty()) {
    ret += " {" + options + "}";
  }
  return ret;
}

std::string SpeechMonitor::Expectation::OptionsToString() const {
  std::vector<std::string> option_str;
  if (as_pattern_) {
    option_str.push_back("pattern: true");
  }
  if (locale_) {
    option_str.push_back("locale: " + locale_.value());
  }
  if (disallowed_text_.size() > 0) {
    option_str.push_back("disallowed: [" +
                         base::JoinString(disallowed_text_, ", ") + "]");
  }
  return base::JoinString(option_str, ", ");
}

SpeechMonitor::SpeechMonitor() {
  content::TtsController::SkipAddNetworkChangeObserverForTests(true);
  content::TtsController::GetInstance()->SetTtsPlatform(this);
}

SpeechMonitor::~SpeechMonitor() {
  content::TtsController::GetInstance()->SetTtsPlatform(
      content::TtsPlatform::GetInstance());
  if (!replay_queue_.empty() || !replayed_queue_.empty())
    CHECK(replay_called_) << "Expectation was made, but Replay() not called.";
}

bool SpeechMonitor::PlatformImplSupported() {
  return true;
}

bool SpeechMonitor::PlatformImplInitialized() {
  return true;
}

void SpeechMonitor::Speak(int utterance_id,
                          const std::string& utterance,
                          const std::string& lang,
                          const content::VoiceData& voice,
                          const content::UtteranceContinuousParameters& params,
                          base::OnceCallback<void(bool)> on_speak_finished) {
  CHECK(!utterance.empty())
      << "If you're deliberately speaking the "
         "empty string in a test, that's probably not the correct way to "
         "achieve stopping speech. If it is unintended, it indicates a deeper "
         "underlying issue.";
  text_params_[utterance] = params;
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id, content::TTS_EVENT_START, 0,
      static_cast<int>(utterance.size()), std::string());

  utterance_ = utterance;
  utterance_id_ = utterance_id;
  on_speak_finished_ = std::move(on_speak_finished);
  if (!send_word_events_and_wait_to_finish_) {
    // finish immediately.
    FinishSpeech();
    return;
  }

  std::size_t space = utterance.find(" ");
  while (space != std::string::npos) {
    // Send word events. This supports some Select-to-Speak tests.
    std::size_t next_space = utterance.find(" ", space + 1);
    int length =
        (next_space == std::string::npos ? utterance.size() : next_space) -
        space;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id, content::TTS_EVENT_WORD, space, length, std::string());
    base::RunLoop().RunUntilIdle();
    space = next_space;
  }
}

void SpeechMonitor::FinishSpeech() {
  CHECK(utterance_id_ != -1)
      << "Cannot FinishSpeech as Speak has not yet been called.";
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, content::TTS_EVENT_END,
      static_cast<int>(utterance_.size()), 0, std::string());
  std::move(on_speak_finished_).Run(true);
  utterance_ = "";
  utterance_id_ = -1;
  on_speak_finished_.Reset();

  time_of_last_utterance_ = std::chrono::steady_clock::now();
}

bool SpeechMonitor::StopSpeaking() {
  ++stop_count_;
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
  voice.engine_id = extension_misc::kGoogleSpeechSynthesisExtensionId;
  voice.events.insert(content::TTS_EVENT_END);
}

void SpeechMonitor::WillSpeakUtteranceWithVoice(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice_data) {
  if (!utterance_queue_.empty() &&
      utterance_queue_.back().text == utterance->GetText() &&
      !base::Contains(repeated_speech_, utterance->GetText())) {
    repeated_speech_.push_back(utterance->GetText());
  }

  utterance_queue_.emplace_back(utterance->GetText(), utterance->GetLang());
  delay_for_last_utterance_ms_ = CalculateUtteranceDelayMS();
  MaybeContinueReplay();
}

void SpeechMonitor::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {}

std::string SpeechMonitor::GetError() {
  return error_;
}

void SpeechMonitor::ClearError() {
  error_ = std::string();
}

void SpeechMonitor::SetError(const std::string& error) {
  error_ = error;
}

void SpeechMonitor::Shutdown() {}

void SpeechMonitor::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {}

void SpeechMonitor::RefreshVoices() {}

content::ExternalPlatformDelegate*
SpeechMonitor::GetExternalPlatformDelegate() {
  return nullptr;
}

double SpeechMonitor::CalculateUtteranceDelayMS() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  std::chrono::duration<double> time_span =
      std::chrono::duration_cast<std::chrono::duration<double>>(
          now - time_of_last_utterance_);
  return time_span.count() * 1000;
}

double SpeechMonitor::GetDelayForLastUtteranceMS() {
  return delay_for_last_utterance_ms_;
}

void SpeechMonitor::ExpectSpeech(const Expectation& expectation,
                                 const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this, expectation]() {
         auto itr = expectation.Matches(utterance_queue_);
         if (itr != utterance_queue_.end()) {
           // Erase all utterances that came before the
           // match as well as the match itself.
           utterance_queue_.erase(utterance_queue_.begin(), itr + 1);
           return true;
         }
         return false;
       },
       "ExpectSpeech(" + expectation.ToString() + ") " + location.ToString()});
}

void SpeechMonitor::ExpectSpeech(const std::string& text,
                                 const base::Location& location) {
  ExpectSpeech(Expectation(text), location);
}

void SpeechMonitor::ExpectSpeechPattern(const std::string& pattern,
                                        const base::Location& location) {
  ExpectSpeech(Expectation(pattern).AsPattern(), location);
}

void SpeechMonitor::ExpectNextSpeechIsNot(const std::string& text,
                                          const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this, text]() {
         if (utterance_queue_.empty())
           return false;

         return text != utterance_queue_.front().text;
       },
       "ExpectNextSpeechIsNot(\"" + text + "\") " + location.ToString()});
}

void SpeechMonitor::ExpectNextSpeechIsNotPattern(
    const std::string& pattern,
    const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back({[this, pattern]() {
                             if (utterance_queue_.empty())
                               return false;

                             return !base::MatchPattern(
                                 utterance_queue_.front().text, pattern);
                           },
                           "ExpectNextSpeechIsNotPattern(\"" + pattern +
                               "\") " + location.ToString()});
}

void SpeechMonitor::ExpectHadNoRepeatedSpeech(const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this]() { return repeated_speech_.empty(); },
       "ExpectHadNoRepeatedSpeech() " + location.ToString()});
}

void SpeechMonitor::Call(std::function<void()> func,
                         const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back({[func]() {
                             func();
                             return true;
                           },
                           "Call() " + location.ToString()});
}

void SpeechMonitor::Replay() {
  replay_called_ = true;
  MaybeContinueReplay();
}

void SpeechMonitor::MaybeContinueReplay() {
  // This method can be called prior to Replay() being called.
  if (!replay_called_)
    return;

  auto it = replay_queue_.begin();
  while (it != replay_queue_.end()) {
    ReplayArgs current = *it;
    it = replay_queue_.erase(it);
    if (current.first()) {
      // Careful here; the above callback may have triggered more speech which
      // causes |MaybeContinueReplay| to be called recursively. We have to
      // ensure to check |replay_queue_| here.
      if (replay_queue_.empty())
        break;

      replayed_queue_.push_back(current.second);
    } else {
      replay_queue_.insert(replay_queue_.begin(), current);
      it = replay_queue_.begin();
      break;
    }
  }

  if (!replay_queue_.empty()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpeechMonitor::MaybePrintExpectations,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kPrintExpectationDelayMs));

    if (!replay_loop_runner_.get()) {
      replay_loop_runner_ = new content::MessageLoopRunner();
      replay_loop_runner_->Run();
    }
  } else if (replay_queue_.empty() && replay_loop_runner_.get()) {
    replay_loop_runner_->Quit();
  }
}

void SpeechMonitor::MaybePrintExpectations() {
  if (CalculateUtteranceDelayMS() < kPrintExpectationDelayMs ||
      replay_queue_.empty())
    return;

  if (last_replay_queue_size_ == replay_queue_.size())
    return;

  last_replay_queue_size_ = replay_queue_.size();
  std::vector<std::string> replay_queue_descriptions;
  for (const auto& pair : replay_queue_)
    replay_queue_descriptions.push_back(pair.second);

  std::vector<std::string> utterance_queue_descriptions;
  for (const auto& item : utterance_queue_)
    utterance_queue_descriptions.push_back("\"" + item.text + "\"");

  std::stringstream output;
  output << "Still waiting for expectation(s).\n";
  if (!replay_queue_descriptions.empty()) {
    output << "Unsatisfied expectations...\n"
           << base::JoinString(replay_queue_descriptions, "\n");
  }
  if (!utterance_queue_descriptions.empty()) {
    output << "\n\npending speech utterances...\n"
           << base::JoinString(utterance_queue_descriptions, "\n");
  }
  if (!replayed_queue_.empty()) {
    output << "\n\nSatisfied expectations...\n"
           << base::JoinString(replayed_queue_, "\n");
  }
  if (!repeated_speech_.empty()) {
    output << "\n\nRepeated speech...\n"
           << base::JoinString(repeated_speech_, "\n");
  }

  LOG(ERROR) << output.str();
}

std::optional<content::UtteranceContinuousParameters>
SpeechMonitor::GetParamsForPreviouslySpokenTextPattern(
    const std::string& pattern) {
  for (const auto& [text, params] : text_params_) {
    if (base::MatchPattern(text, pattern)) {
      return params;
    }
  }
  return std::nullopt;
}

}  // namespace test
}  // namespace ash
