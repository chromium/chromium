// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace speech {
enum class SpeechRecognitionType;
}  // namespace speech

namespace extensions {

// An API function that starts speech recognition.
class SpeechRecognitionPrivateStartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("speechRecognitionPrivate.start",
                             SPEECHRECOGNITIONPRIVATE_START)

 protected:
  ~SpeechRecognitionPrivateStartFunction() override {}
  ResponseAction Run() override;

 private:
  // A callback that is run when the speech recognition service starts.
  void OnStart(speech::SpeechRecognitionType type,
               std::optional<std::string> error);
};

// An API function that stops speech recognition.
class SpeechRecognitionPrivateStopFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("speechRecognitionPrivate.stop",
                             SPEECHRECOGNITIONPRIVATE_STOP)

 protected:
  ~SpeechRecognitionPrivateStopFunction() override {}
  ResponseAction Run() override;

 private:
  // A callback that is run when the speech recognition service stops.
  void OnStop(std::optional<std::string> error);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_API_H_
