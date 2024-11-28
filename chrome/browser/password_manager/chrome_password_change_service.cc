// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "chrome/browser/password_manager/password_change_controller.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "url/gurl.h"

ChromePasswordChangeService::ChromePasswordChangeService(
    affiliations::AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {}

ChromePasswordChangeService::~ChromePasswordChangeService() = default;

bool ChromePasswordChangeService::IsPasswordChangeSupported(const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kImprovedPasswordChangeService)) {
    return false;
  }

  return affiliation_service_->GetChangePasswordURL(url).is_valid();
}

void ChromePasswordChangeService::StartPasswordChange(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
  GURL change_pwd_url = affiliation_service_->GetChangePasswordURL(url);
  CHECK(change_pwd_url.is_valid());

  auto controller = std::make_unique<PasswordChangeController>(
      std::move(change_pwd_url), username, password, web_contents);
  password_change_controllers_.push_back(std::move(controller));
}

bool ChromePasswordChangeService::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return base::ranges::any_of(
      password_change_controllers_,
      [web_contents](
          const std::unique_ptr<PasswordChangeController>& controller) {
        return controller->IsPasswordChangeOngoing(web_contents);
      });
}
