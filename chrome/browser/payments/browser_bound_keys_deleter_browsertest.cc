// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/platform_browser_test.h"
#include "components/payments/content/browser_binding/browser_bound_keys_deleter_factory.h"
#include "components/payments/content/browser_binding/mock_browser_bound_keys_deleter.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

namespace {

using testing::Mock;
using testing::Return;

class BrowserBoundKeysDeleterOnStartupBrowserTest : public PlatformBrowserTest {
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    auto browser_bound_key_deleter_service =
        std::make_unique<payments::MockBrowserBoundKeyDeleter>();
    mock_browser_bound_key_deleter_service_ =
        browser_bound_key_deleter_service.get();
    EXPECT_CALL(*browser_bound_key_deleter_service, RemoveInvalidBBKs);
    BrowserBoundKeyDeleterFactory::GetInstance()->SetServiceForTesting(
        std::move(browser_bound_key_deleter_service));
  }

 protected:
  raw_ptr<MockBrowserBoundKeyDeleter> mock_browser_bound_key_deleter_service_;
};

IN_PROC_BROWSER_TEST_F(BrowserBoundKeysDeleterOnStartupBrowserTest,
                       RemoveInvalidBBKsIsCalled) {
  // The service is already started as part of the profile.
  // Expectations and setup are in SetUpBrowserContextKeyedServices() since they
  // need to be set before the profile is started. However, since the service
  // may live on past the test, explicitly verify the mock here.
  Mock::VerifyAndClearExpectations(mock_browser_bound_key_deleter_service_);
}

}  // namespace

}  // namespace payments
