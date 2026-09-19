// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_INTERACTIVE_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_TTC_CORE_TTC_INTERACTIVE_BROWSER_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/interaction/interactive_browser_test.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ttc {

class TtcKeyedService;

class TtcInteractiveBrowserTestBase : public InteractiveBrowserTest {
 public:
  TtcInteractiveBrowserTestBase();
  ~TtcInteractiveBrowserTestBase() override;

  // InteractiveBrowserTest:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;

 protected:
  Profile* profile();
  TtcKeyedService& ttc_service();
  StepBuilder CheckHasSession(bool expected_has_session);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_INTERACTIVE_BROWSER_TEST_BASE_H_
