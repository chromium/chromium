// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_delegate.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class SpeechRecognitionPrivateRecognizer;

// This class implements core bookkeeping logic for the SpeechRecognitionPrivate
// API. It is responsible for routing API function calls to the correct speech
// recognizer and routing events back to the correct extension.
class SpeechRecognitionPrivateManager
    : public KeyedService,
      public SpeechRecognitionPrivateDelegate {
  using OnStartCallback =
      base::OnceCallback<void(speech::SpeechRecognitionType type,
                              std::optional<std::string> error)>;
  using OnStopCallback =
      base::OnceCallback<void(std::optional<std::string> error)>;

 public:
  explicit SpeechRecognitionPrivateManager(content::BrowserContext* context);
  ~SpeechRecognitionPrivateManager() override;
  SpeechRecognitionPrivateManager(const SpeechRecognitionPrivateManager&) =
      delete;
  SpeechRecognitionPrivateManager& operator=(
      const SpeechRecognitionPrivateManager&) = delete;

  // Creates a unique ID for an API client given an extension ID and an optional
  // ID, which is provided by the client.
  std::string CreateKey(const std::string& extension_id,
                        std::optional<int> client_id);
  // Handles a call to start speech recognition.
  void HandleStart(const std::string& key,
                   std::optional<std::string> locale,
                   std::optional<bool> interim_results,
                   OnStartCallback callback);
  // Handles a call to stop speech recognition.
  void HandleStop(const std::string& key, OnStopCallback callback);

 private:
  friend class SpeechRecognitionPrivateManagerTest;
  friend class SpeechRecognitionPrivateApiTest;

  // SpeechRecognitionPrivateDelegate:
  void HandleSpeechRecognitionStopped(const std::string& key) override;
  void HandleSpeechRecognitionResult(const std::string& key,
                                     const std::u16string& transcript,
                                     bool is_final) override;
  void HandleSpeechRecognitionError(const std::string& key,
                                    const std::string& error) override;

  // Returns the speech recognizer associated with the key. Creates one if
  // none exists.
  SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key);

  // Maps API client IDs to their speech recognizers.
  std::map<std::string, std::unique_ptr<SpeechRecognitionPrivateRecognizer>>
      recognition_data_;

  // The browser context associated with the keyed service.
  raw_ptr<content::BrowserContext> context_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
