// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_FAKE_GAIA_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_FAKE_GAIA_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "google_apis/gaia/fake_gaia.h"

namespace base {
class CommandLine;
}

namespace chromeos {

class FakeGaiaMixin : public InProcessBrowserTestMixin {
 public:
  // Default fake user email and password, may be used by tests.
  static const char kFakeUserEmail[];
  static const char kFakeUserPassword[];
  static const char kFakeUserGaiaId[];
  static const char kFakeAuthCode[];
  static const char kFakeRefreshToken[];
  static const char kEmptyUserServices[];
  static const char kFakeAllScopeAccessToken[];

  // How many seconds until the fake access tokens expire.
  static const int kFakeAccessTokenExpiration;

  // FakeGaia is configured to return these cookies for kFakeUserEmail.
  static const char kFakeSIDCookie[];
  static const char kFakeLSIDCookie[];

  // For obviously consumer users (that have e.g. @gmail.com e-mail) policy
  // fetching code is skipped. This code is executed only for users that may be
  // enterprise users. Thus if you derive from this class and don't need
  // policies, please use @gmail.com e-mail for login. But if you need policies
  // for your test, you must use e-mail addresses that a) have a potentially
  // enterprise domain and b) have been registered with |fake_gaia_|.
  // For your convenience, the e-mail addresses for users that have been set up
  // in this way are provided below.
  static const char kEnterpriseUser1[];
  static const char kEnterpriseUser1GaiaId[];
  static const char kEnterpriseUser2[];
  static const char kEnterpriseUser2GaiaId[];

  static const char kTestUserinfoToken1[];
  static const char kTestRefreshToken1[];
  static const char kTestUserinfoToken2[];
  static const char kTestRefreshToken2[];

  FakeGaiaMixin(InProcessBrowserTestMixinHost* host,
                net::EmbeddedTestServer* embedded_test_server);
  ~FakeGaiaMixin() override;

  // Sets up fake gaia for the login code:
  // - Maps |user_email| to |gaia_id|. If |gaia_id| is empty, |user_email| will
  //   be mapped to kDefaultGaiaId in FakeGaia;
  // - Issues a special all-scope access token associated with the test refresh
  //   token;
  void SetupFakeGaiaForLogin(const std::string& user_email,
                             const std::string& gaia_id,
                             const std::string& refresh_token);
  // Sets up fake gaia to serve access tokens for a child user.
  // *   Maps |user_email| to |gaia_id|. If |gaia_id| is empty, |user_email|
  //     will be mapped to kDefaultGaiaId in FakeGaia.
  // *   Issues user info token scoped for device management service.
  // *   If |issue_any_scope_token|, issues a special all-access token
  //     associated with the test refresh token (as it's done in
  //     SetupFakeGaiaForLogin()).
  // *   Initializes fake merge session as needed.
  void SetupFakeGaiaForChildUser(const std::string& user_email,
                                 const std::string& gaia_id,
                                 const std::string& refresh_token,
                                 bool issue_any_scope_token);
  void SetupFakeGaiaForLoginManager();

  bool initialize_fake_merge_session() {
    return initialize_fake_merge_session_;
  }
  void set_initialize_fake_merge_session(bool value) {
    initialize_fake_merge_session_ = value;
  }

  FakeGaia* fake_gaia() { return fake_gaia_.get(); }
  HTTPSForwarder* gaia_https_forwarder() { return &gaia_https_forwarder_; }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  net::EmbeddedTestServer* embedded_test_server_;
  HTTPSForwarder gaia_https_forwarder_;

  std::unique_ptr<FakeGaia> fake_gaia_;
  bool initialize_fake_merge_session_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeGaiaMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_FAKE_GAIA_MIXIN_H_
