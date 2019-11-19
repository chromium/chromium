// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

class HasEnrolledInstrumentQueryQuotaTest : public PlatformBrowserTest {
 public:
  HasEnrolledInstrumentQueryQuotaTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~HasEnrolledInstrumentQueryQuotaTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_.InitializeAndListen());
    content::SetupCrossSiteRedirector(&https_server_);
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_.StartAcceptingConnections();
    // Cannot use the default localhost hostname, because Chrome turns off the
    // quota for localhost and file:/// scheme to ease web development.
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("a.com", "/has_enrolled_instrument.html")));
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(HasEnrolledInstrumentQueryQuotaTest);
};

class HasEnrolledInstrumentQueryQuotaTestNoFlags
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestNoFlags() {
    features_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{
            features::kStrictHasEnrolledAutofillInstrument,
            features::kWebPaymentsPerMethodCanMakePaymentQuota});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options do not trigger query quota when the strict autofill data
// check is disabled. Per-method query quota is also disabled in this test.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentQueryQuotaTestNoFlags, NoFlags) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

class HasEnrolledInstrumentQueryQuotaTestPerMethodQuota
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestPerMethodQuota() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kWebPaymentsPerMethodCanMakePaymentQuota},
        /*disabled_features=*/{features::kStrictHasEnrolledAutofillInstrument});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options do not trigger query quota when the strict autofill data
// check is disabled. Per-method query quota is enabled in this test.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentQueryQuotaTestPerMethodQuota,
                       PerMethodQuota) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

class HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument},
        /*disabled_features=*/{
            features::kWebPaymentsPerMethodCanMakePaymentQuota});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options trigger query quota for Basic Card when the strict autofill
// data check is enabled. Per-method query quota is disabled in this test.
IN_PROC_BROWSER_TEST_F(
    HasEnrolledInstrumentQueryQuotaTestStrictAutofillDataCheck,
    StrictAutofillDataCheck) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ("NotAllowedError: Exceeded query quota for hasEnrolledInstrument",
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

class HasEnrolledInstrumentQueryQuotaTestBothFlags
    : public HasEnrolledInstrumentQueryQuotaTest {
 public:
  HasEnrolledInstrumentQueryQuotaTestBothFlags() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kStrictHasEnrolledAutofillInstrument,
                              features::
                                  kWebPaymentsPerMethodCanMakePaymentQuota},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Payment options trigger query quota for Basic Card when the strict autofill
// data check is enabled. Per-method query quota is also enabled in this test.
IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentQueryQuotaTestBothFlags,
                       BothFlags) {
  EXPECT_EQ(false,
            content::EvalJs(GetActiveWebContents(), "hasEnrolledInstrument()"));
  EXPECT_EQ("NotAllowedError: Exceeded query quota for hasEnrolledInstrument",
            content::EvalJs(GetActiveWebContents(),
                            "hasEnrolledInstrument({requestShipping:true})"));
}

}  // namespace
}  // namespace payments
