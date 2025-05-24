// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_MOCK_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_MOCK_H_

#include "chrome/browser/password_manager/password_change_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}

class PasswordChangeDelegateMock final : public PasswordChangeDelegate {
 public:
  PasswordChangeDelegateMock();
  PasswordChangeDelegateMock(const PasswordChangeDelegateMock&) = delete;
  PasswordChangeDelegateMock& operator=(const PasswordChangeDelegateMock&) =
      delete;
  ~PasswordChangeDelegateMock() override;

  MOCK_METHOD(void, StartPasswordChangeFlow, (), (override));
  MOCK_METHOD(bool,
              IsPasswordChangeOngoing,
              (content::WebContents*),
              (override));
  MOCK_METHOD(PasswordChangeDelegate::State,
              GetCurrentState,
              (),
              (const override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Restart, (), (override));
#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, OpenPasswordChangeTab, (), (override));
#endif
  MOCK_METHOD(void,
              OnPasswordFormSubmission,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void, OnPrivacyNoticeAccepted, (), (override));
  MOCK_METHOD(void, OnOtpFieldDetected, (content::WebContents*), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(std::u16string, GetDisplayOrigin, (), (const override));
  MOCK_METHOD(const std::u16string&, GetUsername, (), (const override));
  MOCK_METHOD(const std::u16string&,
              GetGeneratedPassword,
              (),
              (const override));

  base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<PasswordChangeDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_MOCK_H_
