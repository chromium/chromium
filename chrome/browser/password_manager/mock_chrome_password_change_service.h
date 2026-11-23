// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MOCK_CHROME_PASSWORD_CHANGE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MOCK_CHROME_PASSWORD_CHANGE_SERVICE_H_

#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#endif

namespace content {
class WebContents;
}

class PasswordChangeDelegate;

class MockChromePasswordChangeService : public ChromePasswordChangeService {
 public:
  MockChromePasswordChangeService();
  ~MockChromePasswordChangeService() override;

  // ChromePasswordChangeService:
  MOCK_METHOD(void,
              OfferPasswordChangeUi,
              (password_manager::LeakedPasswordDetails, content::WebContents*),
              (override));
  MOCK_METHOD(PasswordChangeDelegate*,
              GetPasswordChangeDelegate,
              (content::WebContents*),
              (override));
#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(base::WeakPtr<PasswordChangeFromCheckupDelegate>,
              StartPasswordChangeFromCheckup,
              (password_manager::StoredCredential,
               content::WebContents*,
               PasswordChangeFromCheckupDelegate::StateChangeCallback),
              (override));
#endif
};
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MOCK_CHROME_PASSWORD_CHANGE_SERVICE_H_
