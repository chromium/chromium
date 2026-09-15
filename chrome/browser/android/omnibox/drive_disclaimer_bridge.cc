// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/contextual_search/footprints/public/drive_disclaimer_controller.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/jni_zero/default_conversions.h"

using DisclaimerController = drive_picker::DriveDisclaimerController;
using DisclaimerStatus = DisclaimerController::DisclaimerStatus;

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<DisclaimerStatus>(
    JNIEnv* env,
    const DisclaimerStatus& val) {
  return ToJavaInteger(env, std::to_underlying(val));
}

}  // namespace jni_zero

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/DriveDisclaimerBridge_jni.h"

namespace {

using StatusCallback = base::OnceCallback<void(DisclaimerStatus)>;

contextual_search::DriveConsentState ToConsentState(DisclaimerStatus status) {
  switch (status) {
    case DisclaimerStatus::kAccepted:
      return contextual_search::DriveConsentState::kConsent;
    case DisclaimerStatus::kNotAccepted:
      return contextual_search::DriveConsentState::kNotConsent;
    case DisclaimerStatus::kRestricted:
      return contextual_search::DriveConsentState::kRestricted;
  }
}

void ReportStatus(Profile* profile,
                  StatusCallback callback,
                  std::unique_ptr<DisclaimerController> controller,
                  DisclaimerStatus status) {
  if (profile) {
    profile->GetPrefs()->SetInteger(contextual_search::kDriveConsentState,
                                    std::to_underlying(ToConsentState(status)));
  }
  std::move(callback).Run(status);
}

}  // namespace

// Asynchronously resolves the user's Drive consent status against the
// Footprints (FACS) backend. The backend is the source of truth, since consent
// may have been granted or revoked on another device.
static void JNI_DriveDisclaimerBridge_CheckConsentStatus(
    JNIEnv* env,
    Profile* profile,
    StatusCallback callback) {
  if (!profile) {
    std::move(callback).Run(DisclaimerStatus::kRestricted);
    return;
  }

  // Test override that bypasses the backend round trip entirely.
  if (base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted)) {
    ReportStatus(profile, std::move(callback), /*controller=*/nullptr,
                 DisclaimerStatus::kAccepted);
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    ReportStatus(profile, std::move(callback), /*controller=*/nullptr,
                 DisclaimerStatus::kRestricted);
    return;
  }

  auto controller = std::make_unique<DisclaimerController>(
      contextual_search::FpopService::Create(
          identity_manager, profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess()));
  auto* controller_ptr = controller.get();
  controller_ptr->CheckDisclaimerStatusAsync(base::BindOnce(
      &ReportStatus, profile, std::move(callback), std::move(controller)));
}

DEFINE_JNI(DriveDisclaimerBridge)
