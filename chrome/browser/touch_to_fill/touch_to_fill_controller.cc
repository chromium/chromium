// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/elide_url.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using device_reauth::BiometricsAvailability;
using password_manager::PasswordManagerDriver;
using password_manager::UiCredential;

std::vector<UiCredential> SortCredentials(
    base::span<const UiCredential> credentials) {
  std::vector<UiCredential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) Prefer non-PSL matches over PSL matches.
  // 2) Prefer credentials that were used recently over others.
  //
  // Note: This ordering matches password_manager_util::FindBestMatches().
  base::ranges::sort(result, std::greater<>{}, [](const UiCredential& cred) {
    return std::make_pair(!cred.is_public_suffix_match(), cred.last_used());
  });

  return result;
}

}  // namespace

TouchToFillController::TouchToFillController(
    base::PassKey<TouchToFillControllerTest>,
    scoped_refptr<device_reauth::BiometricAuthenticator> authenticator)
    : authenticator_(std::move(authenticator)) {}

TouchToFillController::TouchToFillController(
    ChromePasswordManagerClient* password_client,
    scoped_refptr<device_reauth::BiometricAuthenticator> authenticator)
    : password_client_(password_client),
      authenticator_(std::move(authenticator)),
      source_id_(ukm::GetSourceIdForWebContentsDocument(
          password_client_->web_contents())) {}

TouchToFillController::~TouchToFillController() {
  if (authenticator_) {
    // This is a noop if no auth triggered by Touch To Fill is in progress.
    authenticator_->Cancel(device_reauth::BiometricAuthRequester::kTouchToFill);
  }
}

void TouchToFillController::Show(base::span<const UiCredential> credentials,
                                 base::WeakPtr<PasswordManagerDriver> driver) {
  DCHECK(!driver_ || driver_.get() == driver.get());
  driver_ = std::move(driver);

  base::UmaHistogramCounts100("PasswordManager.TouchToFill.NumCredentialsShown",
                              credentials.size());

  if (credentials.empty()) {
    // Ideally this should never happen. However, in case we do end up invoking
    // Show() without credentials, we should not show Touch To Fill to the user
    // and treat this case as dismissal, in order to restore the soft keyboard.
    OnDismiss();
    return;
  }

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  const GURL& url = driver_->GetLastCommittedURL();
  view_->Show(
      url,
      TouchToFillView::IsOriginSecure(
          network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url))),
      SortCredentials(credentials));
}

void TouchToFillController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();
  if (!driver_)
    return;

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedCredential))
      .Record(ukm::UkmRecorder::Get());
  if (!password_manager_util::CanUseBiometricAuth(
          authenticator_.get(),
          device_reauth::BiometricAuthRequester::kTouchToFill)) {
    FillCredential(credential);
    return;
  }
  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->Authenticate(
      device_reauth::BiometricAuthRequester::kTouchToFill,
      base::BindOnce(&TouchToFillController::OnReauthCompleted,
                     base::Unretained(this), credential));
}

void TouchToFillController::OnManagePasswordsSelected() {
  view_.reset();
  if (!driver_)
    return;

  std::exchange(driver_, nullptr)
      ->TouchToFillClosed(ShowVirtualKeyboard(false));
  password_client_->NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer::kTouchToFill);

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kManagePasswordsSelected);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedManagePasswords))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  if (!driver_)
    return;

  std::exchange(driver_, nullptr)->TouchToFillClosed(ShowVirtualKeyboard(true));

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kSheetDismissed);
  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kDismissed))
      .Record(ukm::UkmRecorder::Get());
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return password_client_->web_contents()->GetNativeView();
}

void TouchToFillController::OnReauthCompleted(UiCredential credential,
                                              bool auth_successful) {
  if (!driver_)
    return;

  if (!auth_successful) {
    std::exchange(driver_, nullptr)
        ->TouchToFillClosed(ShowVirtualKeyboard(true));
    base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                  TouchToFillOutcome::kReauthenticationFailed);
    return;
  }

  FillCredential(credential);
}

void TouchToFillController::FillCredential(const UiCredential& credential) {
  DCHECK(driver_);

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      credential.is_affiliation_based_match().value());
  driver_->TouchToFillClosed(ShowVirtualKeyboard(false));

  std::exchange(driver_, nullptr)
      ->FillSuggestion(credential.username(), credential.password());

  base::UmaHistogramEnumeration("PasswordManager.TouchToFill.Outcome",
                                TouchToFillOutcome::kCredentialFilled);
}
