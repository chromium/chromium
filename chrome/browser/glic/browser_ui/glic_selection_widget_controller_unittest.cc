// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller_delegate.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace glic {

namespace {

class TestGlicSelectionWidgetControllerDelegate
    : public GlicSelectionWidgetControllerDelegate {
 public:
  // `GlicSelectionWidgetControllerDelegate`:
  content::RenderFrameHost* GetSelectedFrame() const override {
    return nullptr;
  }
  std::optional<gfx::Rect> GetCurrentSelectionBounds() const override {
    return std::nullopt;
  }
  const std::u16string& GetSelectedText() const override {
    return selected_text_;
  }
  bool IsSidePanelOpen() const override { return side_panel_open_; }

  void set_selected_text(const std::u16string& text) { selected_text_ = text; }
  void set_side_panel_open(bool open) { side_panel_open_ = open; }

 private:
  std::u16string selected_text_;
  bool side_panel_open_ = true;
};

class TestGlicSelectionWidgetController : public GlicSelectionWidgetController {
 public:
  using GlicSelectionWidgetController::GlicSelectionWidgetController;

  // `GlicSelectionWidgetController`:
  void Dismiss(DismissReason reason) override {
    dismiss_called_ = true;
    dismiss_reason_ = reason;
    GlicSelectionWidgetController::Dismiss(reason);
  }

  bool dismiss_called() const { return dismiss_called_; }
  std::optional<DismissReason> dismiss_reason() const {
    return dismiss_reason_;
  }

  bool show_selection_overlay_called() const {
    return show_selection_overlay_called_;
  }

  // Expose methods for testing.
  using GlicSelectionWidgetController::ShouldShowSelectionWidget;

 protected:
  // `GlicSelectionWidgetController`:
  void ShowSelectionOverlay() override {
    show_selection_overlay_called_ = true;
    GlicSelectionWidgetController::ShowSelectionOverlay();
  }

 private:
  bool dismiss_called_ = false;
  std::optional<DismissReason> dismiss_reason_;
  bool show_selection_overlay_called_ = false;
};

}  // namespace

class GlicSelectionWidgetControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  // `ChromeRenderViewHostTestHarness`:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    RecreateController();
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void RecreateController() {
    controller_ = std::make_unique<TestGlicSelectionWidgetController>(
        web_contents(), delegate_);
  }

 protected:
  TestGlicSelectionWidgetController* GetController() {
    return controller_.get();
  }

  void set_mock_side_panel_open(bool value) {
    delegate_.set_side_panel_open(value);
  }

  bool ShouldShowSelectionWidget() {
    return controller_->ShouldShowSelectionWidget();
  }

  void CallOnHide() { controller_->OnHide(); }
  void CallOnAskGemini() { controller_->OnAskGemini(); }
  // Sets the selected text, then clicks Ask Gemini.
  void CallOnAskGemini(const std::u16string& selected_text) {
    delegate_.set_selected_text(selected_text);
    controller_->OnAskGemini();
  }

  TestGlicSelectionWidgetControllerDelegate delegate_;
  std::unique_ptr<TestGlicSelectionWidgetController> controller_;
};

TEST_F(GlicSelectionWidgetControllerTest, OnHideHidesSelectionWidget) {
  GURL url("https://example.com");
  NavigateAndCommit(url);
  TestGlicSelectionWidgetController* controller = GetController();
  ASSERT_TRUE(controller);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url, GURL(), ContentSettingsType::INLINE_CUE_MENU));
  EXPECT_TRUE(ShouldShowSelectionWidget());

  CallOnHide();
  EXPECT_FALSE(ShouldShowSelectionWidget());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url, GURL(), ContentSettingsType::INLINE_CUE_MENU));
}

TEST_F(GlicSelectionWidgetControllerTest, SelectionWordCountMetrics) {
  base::HistogramTester histogram_tester;

  std::u16string text = u"   one   two\nthree\t ";
  CallOnAskGemini(text);

  histogram_tester.ExpectUniqueSample(
      "Glic.Selection.WidgetClicked.SelectionLength.PreFre", text.length(), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Selection.WidgetClicked.SelectionWordCount.PreFre", 3, 1);
}

class GlicSelectionWidgetControllerPromptTest
    : public GlicSelectionWidgetControllerTest {
 public:
  // `GlicSelectionWidgetControllerTest`:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/true);
    GlicSelectionWidgetControllerTest::SetUp();
    controller_.reset();

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &GlicSelectionWidgetControllerPromptTest::CreateService,
                       base::Unretained(this)));

    GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
    RecreateController();
  }

  void TearDown() override {
    controller_.reset();
    mock_service_ = nullptr;
    GlicSelectionWidgetControllerTest::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_service_ = service.get();
    return service;
  }

  MockGlicKeyedService* mock_glic_service() { return mock_service_; }

 protected:
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
};

TEST_F(GlicSelectionWidgetControllerPromptTest,
       InvokeGlicFromSelectionAffordanceExplainCta) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt,
        {{"auto_send_prompt", "true"}, {"cta", "explain"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      InvokeWithAutoSubmit(
          testing::_,
          testing::Field(&GlicInvokeOptions::prompts,
                         testing::ElementsAre(l10n_util::GetStringUTF8(
                             IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_EXPLAIN)))))
      .Times(1);

  CallOnAskGemini(u"Sample selected text");
}

TEST_F(GlicSelectionWidgetControllerPromptTest,
       InvokeGlicFromSelectionAffordanceTellMeAboutThisCta) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt,
        {{"auto_send_prompt", "true"}, {"cta", "tell_me_about_this"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      InvokeWithAutoSubmit(
          testing::_,
          testing::Field(&GlicInvokeOptions::prompts,
                         testing::ElementsAre(l10n_util::GetStringUTF8(
                             IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_TELL_ME)))))
      .Times(1);

  CallOnAskGemini(u"Sample selected text");
}

TEST_F(GlicSelectionWidgetControllerPromptTest,
       InvokeGlicFromSelectionAffordanceAutoSendDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt, {{"auto_send_prompt", "false"}}}}, {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      Invoke(testing::Field(&GlicInvokeOptions::prompts, testing::IsEmpty())))
      .Times(1);

  CallOnAskGemini(u"Sample selected text");
}

// The selected text flow has no live mode UI, so its invocations opt out
// rather than pulling a live conversation into the tab's side panel.
TEST_F(GlicSelectionWidgetControllerPromptTest,
       InvokeGlicFromSelectionAffordanceOptsOutOfLiveMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt, {{"auto_send_prompt", "false"}}}}, {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(*mock_glic_service(),
              Invoke(testing::Field(&GlicInvokeOptions::target,
                                    testing::Field(&Target::live_mode_behavior,
                                                   LiveModeBehavior::kFail))))
      .Times(1);

  CallOnAskGemini(u"Sample selected text");
}

TEST_F(GlicSelectionWidgetControllerTest,
       ShouldShowSelectionWidgetSiteBlocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{features::kGlicSelectionDefaultBlockedSites.name,
        "https://blocked-site.com"}});

  NavigateAndCommit(GURL("https://blocked-site.com/page"));
  controller_->OnPrimaryPageChanged();
  EXPECT_FALSE(controller_->ShouldShowSelectionWidget());

  NavigateAndCommit(GURL("https://allowed-site.com/page"));
  controller_->OnPrimaryPageChanged();
  EXPECT_TRUE(controller_->ShouldShowSelectionWidget());

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(GURL("https://allowed-site.com/page"),
                                      GURL("https://allowed-site.com/page"),
                                      ContentSettingsType::INLINE_CUE_MENU,
                                      CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(controller_->ShouldShowSelectionWidget());
}

TEST_F(GlicSelectionWidgetControllerTest, OnAskGeminiWithSmallChipDismissesUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionSmallChip);

  TestGlicSelectionWidgetController* controller = GetController();
  ASSERT_TRUE(controller);

  CallOnAskGemini();

  EXPECT_TRUE(controller->dismiss_called());
  EXPECT_EQ(GlicSelectionObserver::DismissReason::kActionTaken,
            controller->dismiss_reason());
  EXPECT_TRUE(controller->show_selection_overlay_called());
}

TEST_F(GlicSelectionWidgetControllerTest,
       OnAskGeminiWithSmallChipAndOverlayPromptDismissesUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicSelectionSmallChip,
                                 features::kGlicSelectionOverlayPrompt},
                                {});

  TestGlicSelectionWidgetController* controller = GetController();
  ASSERT_TRUE(controller);

  CallOnAskGemini();

  EXPECT_TRUE(controller->dismiss_called());
  EXPECT_EQ(GlicSelectionObserver::DismissReason::kActionTaken,
            controller->dismiss_reason());
  EXPECT_TRUE(controller->show_selection_overlay_called());
}

TEST_F(GlicSelectionWidgetControllerTest,
       OnAskGeminiWithSmallChipWhenSidePanelClosedShowsOverlay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicSelectionSmallChip,
                                 features::kGlicSelectionOverlayPrompt},
                                {});

  TestGlicSelectionWidgetController* controller = GetController();
  ASSERT_TRUE(controller);
  set_mock_side_panel_open(false);

  CallOnAskGemini();

  EXPECT_TRUE(controller->dismiss_called());
  EXPECT_EQ(GlicSelectionObserver::DismissReason::kActionTaken,
            controller->dismiss_reason());
  EXPECT_TRUE(controller->show_selection_overlay_called());
}

}  // namespace glic
