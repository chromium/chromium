// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_BROWSER_TEST_BASE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

class ScalableIphBrowserTestBase : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  feature_engagement::test::MockTracker* mock_tracker() {
    return mock_tracker_;
  }
  test::MockScalableIphDelegate* mock_delegate() { return mock_delegate_; }

 private:
  static void SetTestingFactories(content::BrowserContext* browser_context);
  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* browser_context);
  static std::unique_ptr<scalable_iph::ScalableIphDelegate>
  CreateMockDelegate();

  base::CallbackListSubscription subscription_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  raw_ptr<test::MockScalableIphDelegate> mock_delegate_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_BROWSER_TEST_BASE_H_
