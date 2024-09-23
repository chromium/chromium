// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "components/soda/constants.h"

class SpeechRecognizer;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class SpeechRecognitionPrivateDelegate;

// This class is a wrapper around SpeechRecognizer and can be used to start and
// stop speech recognition. It uses the |delegate| to handle speech recognition
// events. It is also responsible for deciding whether to use the on-device or
// network speech recognition.
class SpeechRecognitionPrivateRecognizer : public SpeechRecognizerDelegate {
  using OnStartCallback =
      base::OnceCallback<void(speech::SpeechRecognitionType type,
                              std::optional<std::string> error)>;
  using OnStopCallback =
      base::OnceCallback<void(std::optional<std::string> error)>;

 public:
  SpeechRecognitionPrivateRecognizer(SpeechRecognitionPrivateDelegate* delegate,
                                     content::BrowserContext* context,
                                     const std::string& id);
  ~SpeechRecognitionPrivateRecognizer() override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(const std::u16string& text,
                      bool is_final,
                      const std::optional<media::SpeechRecognitionResult>&
                          full_result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override {}
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}
  // Handles a call to start speech recognition.
  void HandleStart(std::optional<std::string> locale,
                   std::optional<bool> interim_results,
                   OnStartCallback callback);
  // Handles a call to stop speech recognition. The callback accepts an
  // optional string specifying an error message, if any.
  void HandleStop(OnStopCallback callback);

  std::string locale() { return locale_; }
  bool interim_results() { return interim_results_; }
  SpeechRecognizerStatus current_state() { return current_state_; }

 private:
  friend class SpeechRecognitionPrivateRecognizerTest;

  // Turns the speech recognizer off.
  void RecognizerOff();

  // Updates properties used for speech recognition.
  void MaybeUpdateProperties(std::optional<std::string> locale,
                             std::optional<bool> interim_results,
                             OnStartCallback callback);

  base::WeakPtr<SpeechRecognitionPrivateRecognizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  SpeechRecognizerStatus current_state_ = SPEECH_RECOGNIZER_OFF;
  std::string locale_ = speech::kUsEnglishLocale;
  bool interim_results_ = false;
  // The type of speech recognition being used. Default to kNetwork for
  // initialization purposes. `type_` will always be assigned before speech
  // recognition starts.
  speech::SpeechRecognitionType type_ = speech::SpeechRecognitionType::kNetwork;
  // A callback that is run when speech recognition starts. Note, this is
  // updated whenever HandleStart() is called.
  OnStartCallback on_start_callback_;
  // Delegate that helps handle speech recognition events. `delegate_` is
  // required to outlive this object.
  const raw_ptr<SpeechRecognitionPrivateDelegate> delegate_;
  // The associated BrowserContext.
  const raw_ptr<content::BrowserContext> context_;
  // A unique ID for this speech recognizer.
  const std::string id_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

  base::WeakPtrFactory<SpeechRecognitionPrivateRecognizer> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_
