// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_factory.h"

#include "chrome/browser/payments/browser_binding/mock_browser_bound_key_deleter_service.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

namespace {

using testing::Mock;
using testing::Return;

class BrowserBoundKeyDeleterServiceFactoryBrowserTest
    : public PlatformBrowserTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    // Important for ChromeOS as multiple profiles are created.
    if (service_) {
      return;  // Service already set up.
    }

    auto service =
        std::make_unique<payments::MockBrowserBoundKeyDeleterService>();
    service_ = service.get();

    EXPECT_CALL(*service, RemoveInvalidBBKs);
    BrowserBoundKeyDeleterServiceFactory::GetInstance()->SetServiceForTesting(
        std::move(service));
  }

 protected:
  raw_ptr<MockBrowserBoundKeyDeleterService> service_;
};

IN_PROC_BROWSER_TEST_F(BrowserBoundKeyDeleterServiceFactoryBrowserTest,
                       RemoveInvalidBBKsIsCalled) {
  // The service is already started as part of the profile.
  // Expectations and setup are in SetUpBrowserContextKeyedServices() since they
  // need to be set before the profile is started. However, since the service
  // may live on past the test, explicitly verify the mock here.
  Mock::VerifyAndClearExpectations(service_);

  // Prevents dangling pointer issues as the profile (which owns the service) is
  // destroyed between the end of this test and the destruction of the raw_ptr.
  service_ = nullptr;
}

}  // namespace

}  // namespace payments
