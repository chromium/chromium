// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_DELEGATE_H_

namespace extensions {

// A delegate class that helps the SpeechRecognitionPrivateRecognizer handle
// speech recognition events.
class SpeechRecognitionPrivateDelegate {
 public:
  virtual ~SpeechRecognitionPrivateDelegate() {}

  // Below are methods to handle speech recognition events. `key` is used to
  // specify which SpeechRecognitionPrivateRecognizer this request came from.

  // Called whenever speech recognition stops.
  virtual void HandleSpeechRecognitionStopped(const std::string& key) = 0;
  // Called whenever speech recognition returns a result.
  virtual void HandleSpeechRecognitionResult(const std::string& key,
                                             const std::u16string& transcript,
                                             bool is_final) = 0;
  // Called whenever speech recognition encounters an error.
  virtual void HandleSpeechRecognitionError(const std::string& key,
                                            const std::string& error) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_DELEGATE_H_
