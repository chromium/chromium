// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
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
    base::PassKey<TouchToFillControllerTest>) {}

TouchToFillController::TouchToFillController(
    ChromePasswordManagerClient* password_client)
    : password_client_(password_client),
      source_id_(ukm::GetSourceIdForWebContentsDocument(
          password_client_->web_contents())) {}

TouchToFillController::~TouchToFillController() = default;

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

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      credential.is_affiliation_based_match().value());
  driver_->TouchToFillClosed(ShowVirtualKeyboard(false));
  std::exchange(driver_, nullptr)
      ->FillSuggestion(credential.username(), credential.password());

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedCredential))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillController::OnManagePasswordsSelected() {
  view_.reset();
  if (!driver_)
    return;

  std::exchange(driver_, nullptr)
      ->TouchToFillClosed(ShowVirtualKeyboard(false));
  password_client_->NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer::kTouchToFill);

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedManagePasswords))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  if (!driver_)
    return;

  std::exchange(driver_, nullptr)->TouchToFillClosed(ShowVirtualKeyboard(true));

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kDismissed))
      .Record(ukm::UkmRecorder::Get());
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return password_client_->web_contents()->GetNativeView();
}
