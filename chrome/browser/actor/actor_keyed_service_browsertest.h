// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_BROWSERTEST_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_BROWSERTEST_H_
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace actor {

class ActorKeyedServiceBrowserTest : public PlatformBrowserTest {
 public:
  ActorKeyedServiceBrowserTest();
  ActorKeyedServiceBrowserTest(const ActorKeyedServiceBrowserTest&) = delete;
  ActorKeyedServiceBrowserTest& operator=(const ActorKeyedServiceBrowserTest&) =
      delete;

  ~ActorKeyedServiceBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  tabs::TabInterface* active_tab();
  content::WebContents* web_contents();
  content::RenderFrameHost* main_frame();
  ActorKeyedService* actor_keyed_service();

 private:
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_BROWSERTEST_H_
