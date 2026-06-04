// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicContextMenuInvocationHelperUnittest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();

    profile_ = testing_profile_manager->CreateTestingProfile(
        "test-profile", std::move(testing_factories));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(
                      &GlicContextMenuInvocationHelperUnittest::CreateService,
                      base::Unretained(this)));

    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    // Create the service so mock_service_ is not null.
    GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  }

  void TearDown() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    mock_service_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<MockGlicKeyedService>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_service_ = service.get();
    return service;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler enabler_;
  raw_ptr<TestingProfile> profile_;
  GlicProfileManager glic_profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

// Matcher to check if GlicInvokeOptions has a specific tab as target and
// specific fre_override.
MATCHER_P(TargetTabAndFreOverride, expected, "") {
  auto [expected_tab, expected_fre_override] = expected;
  if (!std::holds_alternative<raw_ptr<tabs::TabInterface>>(
          arg.target.surface)) {
    return false;
  }
  return std::get<raw_ptr<tabs::TabInterface>>(arg.target.surface).get() ==
             expected_tab &&
         arg.fre_override == expected_fre_override;
}

inline auto TargetTabAndFreOverride(
    tabs::TabInterface* expected_tab,
    glic::mojom::FreOverride expected_fre_override) {
  return TargetTabAndFreOverride(
      std::make_pair(expected_tab, expected_fre_override));
}

TEST_F(GlicContextMenuInvocationHelperUnittest, HandleClickStandard) {
  feature_list_.InitWithFeatures({features::kGlic, features::kGlicContextMenu},
                                 {});

  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  EXPECT_CALL(*mock_service_,
              Invoke(TargetTabAndFreOverride(
                  &mock_tab, glic::mojom::FreOverride::kTrustFirstInline)))
      .Times(1);
  GlicContextMenuInvocationHelper::HandleContextualMenuClick(&mock_tab);
}

TEST_F(GlicContextMenuInvocationHelperUnittest, HandleClickArm2) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kGlic, {}},
       {features::kGlicContextMenu,
        {{features::kGlicContextMenuArm.name, "arm2"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  EXPECT_CALL(*mock_service_,
              InvokeWithAutoSubmit(
                  testing::_,
                  TargetTabAndFreOverride(
                      &mock_tab, glic::mojom::FreOverride::kTrustFirstInline)))
      .Times(1);
  GlicContextMenuInvocationHelper::HandleContextualMenuClick(&mock_tab);
}

TEST_F(GlicContextMenuInvocationHelperUnittest, HandleClickArm3) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kGlic, {}},
       {features::kGlicContextMenu,
        {{features::kGlicContextMenuArm.name, "arm3"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  EXPECT_CALL(*mock_service_,
              Invoke(TargetTabAndFreOverride(
                  &mock_tab, glic::mojom::FreOverride::kTrustFirstClick)))
      .Times(1);
  GlicContextMenuInvocationHelper::HandleContextualMenuClick(&mock_tab);
}

TEST_F(GlicContextMenuInvocationHelperUnittest, HandleClickDisabled) {
  // Disable the feature.
  feature_list_.InitWithFeatures({}, {features::kGlicContextMenu});

  // With the feature flag off the menu item would not be added at first place
  // but for completeness sake let's check that the command execution code also
  // returns early.
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  // Should NOT call Invoke.
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);
  GlicContextMenuInvocationHelper::HandleContextualMenuClick(&mock_tab);
}

}  // namespace glic
