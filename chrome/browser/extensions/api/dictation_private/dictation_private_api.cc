// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dictation_private/dictation_private_api.h"

#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/common/extensions/api/dictation_private.h"

namespace extensions {

namespace dictation_private = api::dictation_private;

namespace {

constexpr std::string_view kInvalidStreamIdError = "Invalid stream ID.";
constexpr std::string_view kInvalidAudioLevelError =
    "Invalid audio level (must be between 0.0 and 1.0)";

dictation::StreamProvider::StreamState ConvertStreamState(
    dictation_private::StreamState state) {
  switch (state) {
    case dictation_private::StreamState::kInitializing:
      return dictation::StreamProvider::StreamState::kInitializing;
    case dictation_private::StreamState::kFailed:
      return dictation::StreamProvider::StreamState::kFailed;
    case dictation_private::StreamState::kTranscribing:
      return dictation::StreamProvider::StreamState::kTranscribing;
    case dictation_private::StreamState::kComplete:
      return dictation::StreamProvider::StreamState::kComplete;
    case dictation_private::StreamState::kNone:
      NOTREACHED();
  }
}

}  // namespace

ExtensionFunction::ResponseAction
DictationPrivateUpdateTranscriptionFunction::Run() {
  std::optional<dictation_private::UpdateTranscription::Params> params =
      dictation_private::UpdateTranscription::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  dictation::DictationKeyedService* service =
      dictation::DictationKeyedService::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(service);

  dictation::DictationMultiplexer& multiplexer = service->multiplexer();

  dictation::DictationMultiplexer::StreamId stream_id(
      params->details.stream_id);

  bool is_final =
      params->details.type == dictation_private::TranscriptionType::kFinal;
  if (!multiplexer.UpdateTranscription(stream_id, params->details.data,
                                       is_final)) {
    return RespondNow(Error(std::string(kInvalidStreamIdError)));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DictationPrivateSetStreamStateFunction::Run() {
  std::optional<dictation_private::SetStreamState::Params> params =
      dictation_private::SetStreamState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  dictation::DictationKeyedService* service =
      dictation::DictationKeyedService::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(service);

  dictation::DictationMultiplexer& multiplexer = service->multiplexer();

  dictation::DictationMultiplexer::StreamId stream_id(
      params->details.stream_id);

  if (!multiplexer.SetStreamState(stream_id,
                                  ConvertStreamState(params->details.state))) {
    return RespondNow(Error(std::string(kInvalidStreamIdError)));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DictationPrivateUpdateAudioLevelFunction::Run() {
  std::optional<dictation_private::UpdateAudioLevel::Params> params =
      dictation_private::UpdateAudioLevel::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  auto* service = dictation::DictationKeyedService::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(service);

  if (params->audio_level < 0.0 || params->audio_level > 1.0) {
    return RespondNow(Error(std::string(kInvalidAudioLevelError)));
  }

  service->UpdateAudioLevel(base::saturated_cast<float>(params->audio_level));

  return RespondNow(NoArguments());
}

}  // namespace extensions
