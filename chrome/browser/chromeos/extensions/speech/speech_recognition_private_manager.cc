// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/speech_recognition_private.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

SpeechRecogntionPrivateManager::SpeechRecogntionPrivateManager() = default;
SpeechRecogntionPrivateManager::~SpeechRecogntionPrivateManager() = default;

SpeechRecogntionPrivateManager* SpeechRecogntionPrivateManager::GetInstance() {
  static base::NoDestructor<SpeechRecogntionPrivateManager> instance;
  return instance.get();
}

void SpeechRecogntionPrivateManager::HandleStart(
    const std::string& key,
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    base::OnceClosure on_start_callback) {
  GetSpeechRecognizer(key)->HandleStart(locale, interim_results,
                                        std::move(on_start_callback));
}

std::string SpeechRecogntionPrivateManager::CreateKey(
    const std::string& extension_id,
    absl::optional<int> client_id) {
  if (!client_id.has_value())
    return extension_id;

  return base::StringPrintf("%s.%d", extension_id.c_str(), *client_id);
}

SpeechRecognitionPrivateRecognizer*
SpeechRecogntionPrivateManager::GetSpeechRecognizer(const std::string& key) {
  auto& recognizer = recognition_data_[key];
  if (!recognizer)
    recognizer = std::make_unique<SpeechRecognitionPrivateRecognizer>();

  return recognizer.get();
}

}  // namespace extensions
