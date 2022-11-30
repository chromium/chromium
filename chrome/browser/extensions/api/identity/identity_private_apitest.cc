// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_private_api.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

struct SetConsentResultParams {
  std::string consent_result;
  std::string window_id;
};

class IdentityPrivateApiTest : public ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    callback_loop_ = std::make_unique<base::RunLoop>();
    // base::Unretained(this) is safe because the callback will be unregistered
    // on |callback_subscription_| destruction.
    callback_subscription_ = identity_api()->RegisterOnSetConsentResultCallback(
        base::BindRepeating(&IdentityPrivateApiTest::OnSetConsentResult,
                            base::Unretained(this)));
  }

  IdentityAPI* identity_api() {
    return IdentityAPI::GetFactoryInstance()->Get(profile());
  }

  SetConsentResultParams WaitForConsentResult() {
    callback_loop_->Run();
    return {consent_result_, window_id_};
  }

 private:
  void OnSetConsentResult(const std::string& consent_result,
                          const std::string& window_id) {
    consent_result_ = consent_result;
    window_id_ = window_id;
    callback_loop_->Quit();
  }

  std::string consent_result_;
  std::string window_id_;
  std::unique_ptr<base::RunLoop> callback_loop_;
  base::CallbackListSubscription callback_subscription_;
};

IN_PROC_BROWSER_TEST_F(IdentityPrivateApiTest, SetConsentResult) {
  scoped_refptr<ExtensionFunction> func =
      base::MakeRefCounted<IdentityPrivateSetConsentResultFunction>();
  bool success = api_test_utils::RunFunction(
      func.get(),
      std::string("[\"consent_result_value\", \"window_id_value\"]"),
      profile());
  ASSERT_TRUE(success);
  SetConsentResultParams params = WaitForConsentResult();
  EXPECT_EQ(params.consent_result, "consent_result_value");
  EXPECT_EQ(params.window_id, "window_id_value");
}

}  // namespace extensions
