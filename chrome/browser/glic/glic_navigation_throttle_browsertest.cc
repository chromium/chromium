// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace glic {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  explicit MockGlicKeyedService(content::BrowserContext* context)
      : GlicKeyedService(
            Profile::FromBrowserContext(context),
            IdentityManagerFactory::GetForProfile(
                Profile::FromBrowserContext(context)),
            g_browser_process->profile_manager(),
            GlicProfileManager::GetInstance(),
            contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
                Profile::FromBrowserContext(context)),
            actor::ActorKeyedServiceFactory::GetActorKeyedService(context)) {}
  MOCK_METHOD(void,
              ShowUiWithConversationID,
              (BrowserWindowInterface*, mojom::InvocationSource, std::string),
              (override));
};

std::unique_ptr<KeyedService> CreateMockGlicKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<MockGlicKeyedService>(context);
}

class GlicNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  GlicNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicWebContinuity}, {});

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicNavigationThrottleBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));

    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateMockGlicKeyedService));
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicNavigationThrottleBrowserTest,
                       InterceptGlicContinueUrlFromGemini) {
  MockGlicKeyedService* mock_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile(),
                                                   /*create=*/true));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, ShowUiWithConversationID(
                                 _, mojom::InvocationSource::kNavigationCapture,
                                 std::string("123")))
      .Times(1);

  GURL target_url("https://www.google.com/");
  GURL continue_url(
      "https://gemini.google.com/glic/"
      "continue?cid=123&targetUrl=" +
      target_url.spec());

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::NavigationController::LoadURLParams params(continue_url);
  params.initiator_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .LoadURLWithParams(params);
  observer.Wait();

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            target_url);
}

}  // namespace glic
