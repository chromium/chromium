// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_util.h"

#include <string>

#include "media/mojo/mojom/speech_recognizer.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/soda/soda_util.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kEnglishLanguageCode[] = "en";
}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace speech {

media::mojom::AvailabilityStatus GetOnDeviceSpeechRecognitionAvailabilityStatus(
    content::BrowserContext* context,
    const std::string& language) {
#if BUILDFLAG(IS_ANDROID)
  return media::mojom::AvailabilityStatus::kUnavailable;
#else
  if (base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano)) {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));

    if (!service) {
      return media::mojom::AvailabilityStatus::kUnavailable;
    }

    // TODO(crbug.com/446260680): Add support for other languages.
    if (l10n_util::GetLanguage(language) != kEnglishLanguageCode) {
      return media::mojom::AvailabilityStatus::kUnavailable;
    }

    // TODO(crbug.com/446260680): Use
    // OnDeviceFeature::kOnDeviceSpeechRecognition.
    std::optional<optimization_guide::mojom::ModelUnavailableReason>
        unavailable_reason =
            optimization_guide::AvailabilityFromEligibilityReason(
                service->GetOnDeviceModelEligibility(
                    optimization_guide::mojom::OnDeviceFeature::kPromptApi));
    if (!unavailable_reason.has_value()) {
      return media::mojom::AvailabilityStatus::kAvailable;
    } else {
      if (unavailable_reason.value() ==
              optimization_guide::mojom::ModelUnavailableReason::
                  kPendingAssets ||
          unavailable_reason.value() ==
              optimization_guide::mojom::ModelUnavailableReason::
                  kPendingUsage) {
        return media::mojom::AvailabilityStatus::kDownloadable;
      } else {
        return media::mojom::AvailabilityStatus::kUnavailable;
      }
    }
  }

  return GetSodaAvailabilityStatus(language);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace speech
