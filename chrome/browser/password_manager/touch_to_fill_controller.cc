// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/util/type_safety/pass_key.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "components/favicon/core/favicon_service.h"
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

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using password_manager::CredentialPair;
using password_manager::PasswordManagerDriver;

namespace {

void OnImageFetched(base::OnceCallback<void(const gfx::Image&)> callback,
                    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  gfx::Image image;
  if (bitmap_result.is_valid())
    image = gfx::Image::CreateFrom1xPNGBytes(bitmap_result.bitmap_data);
  std::move(callback).Run(image);
}

}  // namespace

TouchToFillController::TouchToFillController(
    util::PassKey<TouchToFillControllerTest>) {}

TouchToFillController::TouchToFillController(
    ChromePasswordManagerClient* password_client,
    favicon::FaviconService* favicon_service)
    : password_client_(password_client),
      favicon_service_(favicon_service),
      source_id_(ukm::GetSourceIdForWebContentsDocument(
          password_client_->web_contents())) {}

TouchToFillController::~TouchToFillController() = default;

void TouchToFillController::Show(base::span<const CredentialPair> credentials,
                                 base::WeakPtr<PasswordManagerDriver> driver) {
  DCHECK(!driver_ || driver_.get() == driver.get());
  driver_ = std::move(driver);

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  const GURL& url = driver_->GetLastCommittedURL();
  view_->Show(url,
              TouchToFillView::IsOriginSecure(
                  network::IsUrlPotentiallyTrustworthy(url)),
              credentials);
}

void TouchToFillController::OnCredentialSelected(
    const CredentialPair& credential) {
  if (!driver_)
    return;

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      password_manager::IsValidAndroidFacetURI(credential.origin_url.spec()));
  driver_->TouchToFillClosed(ShowVirtualKeyboard(false));
  std::exchange(driver_, nullptr)
      ->FillSuggestion(credential.username, credential.password);

  ukm::builders::TouchToFill_Shown(source_id_)
      .SetUserAction(static_cast<int64_t>(UserAction::kSelectedCredential))
      .Record(ukm::UkmRecorder::Get());
}

void TouchToFillController::OnManagePasswordsSelected() {
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

void TouchToFillController::FetchFavicon(
    const GURL& credential_origin,
    const GURL& frame_origin,
    int desired_size_in_pixel,
    base::OnceCallback<void(const gfx::Image&)> callback) {
  favicon_service_->GetRawFaviconForPageURL(
      url::Origin::Create(credential_origin).opaque() ? frame_origin
                                                      : credential_origin,
      {favicon_base::IconType::kFavicon}, desired_size_in_pixel,
      /* fallback_to_host = */ true,
      base::BindOnce(&OnImageFetched, std::move(callback)), &favicon_tracker_);
}
