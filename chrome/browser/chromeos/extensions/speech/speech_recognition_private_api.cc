// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_api.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/common/extensions/api/speech_recognition_private.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

ExtensionFunction::ResponseAction SpeechRecognitionPrivateStartFunction::Run() {
  // TODO(crbug.com/1246048): Add error handling for multiple calls to start().
  // If start() is called while speech recognition is already happening, we
  // should throw an error.

  // Extract arguments.
  std::unique_ptr<api::speech_recognition_private::Start::Params> params(
      api::speech_recognition_private::Start::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::speech_recognition_private::StartOptions* options =
      &params->options;
  DCHECK(options);
  absl::optional<int> client_id;
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results;
  if (options->client_id)
    client_id = *options->client_id;
  if (options->locale)
    locale = *options->locale;
  if (options->interim_results)
    interim_results = *options->interim_results;

  // Get the unique key for this API client and ask the manager to handle this
  // API call.
  SpeechRecogntionPrivateManager* manager =
      SpeechRecogntionPrivateManager::GetInstance();
  const std::string key = manager->CreateKey(extension_id(), client_id);
  manager->HandleStart(
      key, locale, interim_results,
      base::BindOnce(&SpeechRecognitionPrivateStartFunction::OnStart,
                     base::RetainedRef(this)));
  return RespondLater();
}

void SpeechRecognitionPrivateStartFunction::OnStart() {
  Respond(NoArguments());
}

}  // namespace extensions
