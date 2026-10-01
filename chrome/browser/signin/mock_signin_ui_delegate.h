// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_MOCK_SIGNIN_UI_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_MOCK_SIGNIN_UI_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace signin_ui_util {

class MockSigninUiDelegate : public SigninUiDelegate {
 public:
  MockSigninUiDelegate();
  ~MockSigninUiDelegate();

  MockSigninUiDelegate(const MockSigninUiDelegate&) = delete;
  MockSigninUiDelegate& operator=(const MockSigninUiDelegate&) = delete;

  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile*,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction,
               const CoreAccountId&,
               TurnSyncOnHelper::SigninAbortedMode,
               bool,
               bool),
              (override));
  MOCK_METHOD(void,
              ShowHistorySyncOptinUI,
              (Profile*, const CoreAccountId&, signin_metrics::AccessPoint),
              (override));
  MOCK_METHOD(void,
              ShowCrossDeviceSigninQrBubble,
              (BrowserWindowInterface*,
               GURL,
               base::OnceClosure,
               CrossDeviceSigninPromoEntryPoint),
              (override));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
};

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_MOCK_SIGNIN_UI_DELEGATE_H_
