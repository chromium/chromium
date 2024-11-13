// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

ChromePasswordChangeService::ChromePasswordChangeService(
    affiliations::AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {}

bool ChromePasswordChangeService::IsPasswordChangeSupported(const GURL& url) {
  NOTIMPLEMENTED();
  return false;
}
void ChromePasswordChangeService::StartPasswordChange(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
}
bool ChromePasswordChangeService::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return false;
}
