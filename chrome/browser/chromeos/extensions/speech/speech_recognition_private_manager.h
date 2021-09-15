// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class SpeechRecognitionPrivateRecognizer;

// This class implements core bookkeeping logic for the SpeechRecognitionPrivate
// API. It is responsible for routing API function calls to the correct speech
// recognizer and routing events back to the correct extension.
class SpeechRecogntionPrivateManager {
 public:
  SpeechRecogntionPrivateManager();
  ~SpeechRecogntionPrivateManager();
  SpeechRecogntionPrivateManager(const SpeechRecogntionPrivateManager&) =
      delete;
  SpeechRecogntionPrivateManager& operator=(
      const SpeechRecogntionPrivateManager&) = delete;

  static SpeechRecogntionPrivateManager* GetInstance();
  // Creates a unique ID for an API client given an extension ID and an optional
  // ID, which is provided by the client.
  std::string CreateKey(const std::string& extension_id,
                        absl::optional<int> client_id);
  // Handles a call to start speech recognition.
  void HandleStart(const std::string& key,
                   absl::optional<std::string> locale,
                   absl::optional<bool> interim_results,
                   base::OnceClosure on_start_callback);

 private:
  friend class SpeechRecognitionPrivateManagerTest;
  friend class SpeechRecognitionPrivateApiTest;

  // Returns the speech recognizer associated with the key. Creates one if
  // none exists.
  SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key);

  // Maps API client IDs to their speech recognizers.
  std::map<std::string, std::unique_ptr<SpeechRecognitionPrivateRecognizer>>
      recognition_data_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
