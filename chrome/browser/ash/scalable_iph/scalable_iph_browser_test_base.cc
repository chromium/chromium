// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

void ScalableIphBrowserTestBase::SetUp() {
  // Keyed service is a service which is tied to an object. For our use cases,
  // the object is `BrowserContext` (e.g. `Profile`). See
  // //components/keyed_service/README.md for details on keyed service.
  //
  // We set a testing factory to inject a mock. A testing factory must be set
  // early enough as a service is not created before that, e.g. a `Tracker` must
  // not be created before we set `CreateMockTracker`. If a keyed service is
  // created before we set our testing factory, `SetTestingFactory` will
  // destruct already created keyed services at a time we set our testing
  // factory. It destructs a keyed service at an unusual timing. It can trigger
  // a dangling pointer issue, etc.
  //
  // `SetUpOnMainThread` below is too late to set a testing factory. Note that
  // `InProcessBrowserTest::SetUp` is called at the very early stage, e.g.
  // before command lines are set, etc.
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&ScalableIphBrowserTestBase::CreateServices));

  InProcessBrowserTest::SetUp();
}

// `SetUpOnMainThread` is called just before a test body. Do the mock set up in
// this function as `browser()` is not available in `SetUp` above.
void ScalableIphBrowserTestBase::SetUpOnMainThread() {
  mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->profile()));
  CHECK(mock_tracker_)
      << "mock_tracker_ must be non-nullptr. GetForBrowserContext should "
         "create one via CreateMockTracker if it does not exist.";

  ON_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillByDefault(
          [](feature_engagement::Tracker::OnInitializedCallback callback) {
            std::move(callback).Run(true);
          });

  ON_CALL(*mock_tracker_, IsInitialized).WillByDefault(testing::Return(true));

  InProcessBrowserTest::SetUpOnMainThread();
}

void ScalableIphBrowserTestBase::TearDownOnMainThread() {
  // We are going to release a reference to a MockTracker below. Verify the
  // expectation in advance to have a predictable behavior.
  testing::Mock::VerifyAndClearExpectations(mock_tracker_);
  mock_tracker_ = nullptr;

  InProcessBrowserTest::TearDownOnMainThread();
}

// static
void ScalableIphBrowserTestBase::CreateServices(
    content::BrowserContext* browser_context) {
  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      browser_context,
      base::BindRepeating(&ScalableIphBrowserTestBase::CreateMockTracker));
}

// static
std::unique_ptr<KeyedService> ScalableIphBrowserTestBase::CreateMockTracker(
    content::BrowserContext* browser_context) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace ash
