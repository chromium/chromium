// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class SpeechRecognitionPrivateRecognizer;

// This class implements core bookkeeping logic for the SpeechRecognitionPrivate
// API. It is responsible for routing API function calls to the correct speech
// recognizer and routing events back to the correct extension.
class SpeechRecognitionPrivateManager : public KeyedService {
 public:
  explicit SpeechRecognitionPrivateManager(content::BrowserContext* context);
  ~SpeechRecognitionPrivateManager() override;
  SpeechRecognitionPrivateManager(const SpeechRecognitionPrivateManager&) =
      delete;
  SpeechRecognitionPrivateManager& operator=(
      const SpeechRecognitionPrivateManager&) = delete;

  // Gets or creates an instance of SpeechRecognitionPrivateManager from a
  // browser context.
  static SpeechRecognitionPrivateManager* Get(content::BrowserContext* context);

  // Creates a unique ID for an API client given an extension ID and an optional
  // ID, which is provided by the client.
  std::string CreateKey(const std::string& extension_id,
                        absl::optional<int> client_id);
  // Handles a call to start speech recognition.
  void HandleStart(const std::string& key,
                   absl::optional<std::string> locale,
                   absl::optional<bool> interim_results,
                   base::OnceClosure on_start_callback);
  // Handles a call to stop speech recognition.
  void HandleStop(
      const std::string& key,
      base::OnceCallback<void(absl::optional<std::string>)> on_stop_callback);

 private:
  friend class SpeechRecognitionPrivateManagerTest;
  friend class SpeechRecognitionPrivateApiTest;

  // Dispatches an event when speech recognition stops in the background without
  // an explicit call to HandleStop() e.g. when speech recognition encounters
  // a fatal error.
  void DispatchOnStopEvent(const std::string& key);
  // Dispatches an event when speech recognition returns a result.
  void DispatchOnResultEvent(const std::string& key,
                             const std::u16string& transcript,
                             bool is_final);
  // Dispatches an event when a speech recognition error occurs.
  void DispatchOnErrorEvent(const std::string& key, const std::string& message);

  // Retrieves the factory instance for SpeechRecognitionPrivateManager.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns the speech recognizer associated with the key. Creates one if
  // none exists.
  SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key);

  base::WeakPtr<SpeechRecognitionPrivateManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Maps API client IDs to their speech recognizers.
  std::map<std::string, std::unique_ptr<SpeechRecognitionPrivateRecognizer>>
      recognition_data_;

  // The browser context associated with the keyed service.
  content::BrowserContext* context_;

  base::WeakPtrFactory<SpeechRecognitionPrivateManager> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_H_
