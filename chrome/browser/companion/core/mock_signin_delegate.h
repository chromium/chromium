// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_MOCK_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_COMPANION_CORE_MOCK_SIGNIN_DELEGATE_H_

#include "chrome/browser/companion/core/signin_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace companion {

class MockSigninDelegate : public SigninDelegate {
 public:
  MockSigninDelegate();
  ~MockSigninDelegate() override;

  // Disallow copy/assign.
  MockSigninDelegate(const MockSigninDelegate&) = delete;
  MockSigninDelegate& operator=(const MockSigninDelegate&) = delete;

  MOCK_METHOD0(AllowedSignin, bool());
  MOCK_METHOD0(IsSignedIn, bool());
  MOCK_METHOD0(StartSigninFlow, void());
  MOCK_METHOD1(EnableMsbb, void(bool));
  MOCK_METHOD2(OpenUrlInBrowser, void(const GURL&, bool));
  MOCK_METHOD0(ShouldShowRegionSearchIPH, bool());
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_MOCK_SIGNIN_DELEGATE_H_
