// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_util.h"

#include <string>

#include "media/mojo/mojom/speech_recognizer.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/soda/soda_util.h"  // nogncheck crbug.com/40147906
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kEnglishLanguageCode[] = "en";

std::optional<optimization_guide::mojom::OnDeviceFeature> PickOnDeviceFeature(
    media::mojom::SpeechRecognitionQuality quality) {
  if ((quality == media::mojom::SpeechRecognitionQuality::kConversation &&
       !base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano)) ||
      (quality == media::mojom::SpeechRecognitionQuality::kDictation &&
       !base::FeatureList::IsEnabled(
           media::kOnDeviceWebSpeechSmallExpertModel))) {
    return std::nullopt;
  }

  return quality == media::mojom::SpeechRecognitionQuality::kDictation
             ? optimization_guide::mojom::OnDeviceFeature::
                   kSpeechRecognitionSmallExpertModel
             : optimization_guide::mojom::OnDeviceFeature::
                   kOnDeviceSpeechRecognition;
}

struct FeatureAndService {
  optimization_guide::mojom::OnDeviceFeature feature;
  raw_ptr<OptimizationGuideKeyedService> service = nullptr;
};

std::optional<FeatureAndService> GetOnDeviceFeatureAndService(
    content::BrowserContext* context,
    std::string_view language,
    media::mojom::SpeechRecognitionQuality quality) {
  std::optional<optimization_guide::mojom::OnDeviceFeature> feature =
      PickOnDeviceFeature(quality);
  if (!feature) {
    return std::nullopt;
  }

  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!service) {
    return std::nullopt;
  }

  if (base::FeatureList::IsEnabled(
          media::kOnDeviceWebSpeechSmallExpertModelMultiLanguage) &&
      quality == media::mojom::SpeechRecognitionQuality::kDictation) {
    std::string languages_str =
        media::kOnDeviceWebSpeechSmallExpertModelLanguages.Get();
    std::vector<std::string> enabled_languages = base::SplitString(
        languages_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    bool is_language_supported = false;
    for (const std::string& enabled_language : enabled_languages) {
      if (l10n_util::GetLanguage(language) ==
          l10n_util::GetLanguage(enabled_language)) {
        is_language_supported = true;
        break;
      }
    }

    if (!is_language_supported) {
      return std::nullopt;
    }
  } else if (l10n_util::GetLanguage(language) != kEnglishLanguageCode) {
    return std::nullopt;
  }

  return FeatureAndService{feature.value(), service};
}

media::mojom::AvailabilityStatus ModelUnavailableReasonToAvailabilityStatus(
    optimization_guide::OnDeviceModelEligibilityReason eligibility_reason) {
  std::optional<optimization_guide::mojom::ModelUnavailableReason>
      unavailable_reason =
          optimization_guide::AvailabilityFromEligibilityReason(
              eligibility_reason);
  if (!unavailable_reason.has_value()) {
    return media::mojom::AvailabilityStatus::kAvailable;
  }
  if (unavailable_reason ==
      optimization_guide::mojom::ModelUnavailableReason::kNotSupported) {
    return media::mojom::AvailabilityStatus::kUnavailable;
  }
  return media::mojom::AvailabilityStatus::kDownloadable;
}

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace speech {

media::mojom::AvailabilityStatus GetOnDeviceSpeechRecognitionAvailabilityStatus(
    content::BrowserContext* context,
    std::string_view language,
    media::mojom::SpeechRecognitionQuality quality) {
#if BUILDFLAG(IS_ANDROID)
  return media::mojom::AvailabilityStatus::kUnavailable;
#else
  if (quality == media::mojom::SpeechRecognitionQuality::kConversation ||
      quality == media::mojom::SpeechRecognitionQuality::kDictation) {
    auto feature_and_service =
        GetOnDeviceFeatureAndService(context, language, quality);
    if (!feature_and_service) {
      return media::mojom::AvailabilityStatus::kUnavailable;
    }

    return ModelUnavailableReasonToAvailabilityStatus(
        feature_and_service->service->GetOnDeviceModelEligibility(
            feature_and_service->feature));
  }

  return GetSodaAvailabilityStatus(language);
#endif  // BUILDFLAG(IS_ANDROID)
}

void GetOnDeviceSpeechRecognitionAvailabilityStatusAsync(
    content::BrowserContext* context,
    std::string_view language,
    media::mojom::SpeechRecognitionQuality quality,
    base::OnceCallback<void(media::mojom::AvailabilityStatus)> callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
#else
  if (quality == media::mojom::SpeechRecognitionQuality::kConversation ||
      quality == media::mojom::SpeechRecognitionQuality::kDictation) {
    auto feature_and_service =
        GetOnDeviceFeatureAndService(context, language, quality);
    if (!feature_and_service) {
      std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
      return;
    }

    feature_and_service->service->GetOnDeviceModelEligibilityAsync(
        feature_and_service->feature, /*capabilities=*/{},
        base::BindOnce(
            [](base::OnceCallback<void(media::mojom::AvailabilityStatus)>
                   callback,
               optimization_guide::OnDeviceModelEligibilityReason
                   eligibility_reason) {
              std::move(callback).Run(
                  ModelUnavailableReasonToAvailabilityStatus(
                      eligibility_reason));
            },
            std::move(callback)));
    return;
  }

  std::move(callback).Run(GetSodaAvailabilityStatus(language));
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace speech
