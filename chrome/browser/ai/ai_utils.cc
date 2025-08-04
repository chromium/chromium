// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_utils.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

blink::mojom::ModelStreamingResponseStatus AIUtils::ConvertModelExecutionError(
    ModelExecutionError error) {
  switch (error) {
    case ModelExecutionError::kUnknown:
      return blink::mojom::ModelStreamingResponseStatus::kErrorUnknown;
    case ModelExecutionError::kInvalidRequest:
      return blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest;
    case ModelExecutionError::kRequestThrottled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRequestThrottled;
    case ModelExecutionError::kPermissionDenied:
      return blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied;
    case ModelExecutionError::kGenericFailure:
      return blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure;
    case ModelExecutionError::kRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRetryableError;
    case ModelExecutionError::kNonRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorNonRetryableError;
    case ModelExecutionError::kUnsupportedLanguage:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorUnsupportedLanguage;
    case ModelExecutionError::kFiltered:
      return blink::mojom::ModelStreamingResponseStatus::kErrorFiltered;
    case ModelExecutionError::kDisabled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorDisabled;
    case ModelExecutionError::kCancelled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorCancelled;
    case ModelExecutionError::kResponseLowQuality:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorResponseLowQuality;
  }
}

// static
int64_t AIUtils::NormalizeModelDownloadProgress(int64_t bytes_so_far,
                                                int64_t total_bytes) {
  // If `bytes_so_far` is zero, we should have downloaded zero bytes
  // out of zero meaning we're at 100%. So set it to
  // `kNormalizedDownloadProgressMax` to avoid dividing by zero.
  if (total_bytes == 0) {
    CHECK_EQ(bytes_so_far, 0);
    return AIUtils::kNormalizedDownloadProgressMax;
  }

  double raw_progress_fraction =
      bytes_so_far / static_cast<double>(total_bytes);

  return raw_progress_fraction * AIUtils::kNormalizedDownloadProgressMax;
}

// static
base::flat_set<std::string_view> AIUtils::RestrictSupportedLanguagesForFeature(
    const base::flat_set<std::string_view>& supported,
    const base::FeatureParam<std::string>& feature_param) {
  if (feature_param.Get() == "*") {
    return base::MakeFlatSet<std::string_view>(supported);
  }

  std::vector<std::string> enabled_languages =
      base::SplitString(feature_param.Get(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  std::vector<std::string> difference;
  std::set_difference(enabled_languages.begin(), enabled_languages.end(),
                      supported.begin(), supported.end(),
                      std::back_inserter(difference));
  if (!difference.empty()) {
    LOG(WARNING) << "Enabled languages (" << base::JoinString(difference, ", ")
                 << ") are not supported for " << feature_param.feature->name;
  }

  base::flat_set<std::string_view> supported_languages;
  std::set_intersection(
      supported.begin(), supported.end(), enabled_languages.begin(),
      enabled_languages.end(),
      std::inserter(supported_languages, supported_languages.end()));

  LOG_IF(WARNING, supported_languages.empty())
      << "Supported languages is empty: " << feature_param.feature->name;

  return supported_languages;
}
