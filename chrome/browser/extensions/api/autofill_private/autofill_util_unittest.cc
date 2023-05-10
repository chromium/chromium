// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"

using testing::Return;

namespace extensions::autofill_util {

class AutofillUtilTest : public InProcessBrowserTest {
 public:
  AutofillUtilTest() = default;

  AutofillUtilTest(const AutofillUtilTest&) = delete;
  AutofillUtilTest& operator=(const AutofillUtilTest&) = delete;

  void SetUpOnMainThread() override {
    mock_device_authenticator_ =
        base::MakeRefCounted<device_reauth::MockDeviceAuthenticator>();
  }

 protected:
  scoped_refptr<device_reauth::MockDeviceAuthenticator>
      mock_device_authenticator_;
};

IN_PROC_BROWSER_TEST_F(
    AutofillUtilTest,
    AuthenticateUserOnMandatoryReuathToggled_SuccessfulAuth) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;

  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(result_callback, Run(true));

  AuthenticateUserOnMandatoryReauthToggled(mock_device_authenticator_,
                                           result_callback.Get());
#endif
}

IN_PROC_BROWSER_TEST_F(
    AutofillUtilTest,
    AuthenticateUserOnMandatoryReuathToggled_UnSuccessfulAuth) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;

  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(result_callback, Run(false));

  AuthenticateUserOnMandatoryReauthToggled(mock_device_authenticator_,
                                           result_callback.Get());
#endif
}

}  // namespace extensions::autofill_util
