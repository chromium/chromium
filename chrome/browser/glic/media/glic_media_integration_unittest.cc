// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace glic {

class GlicMediaIntegrationTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Return a mock Live Caption controller.
    auto controller = CreateLiveCaptionController();
    live_caption_controller_ = controller.get();
    captions::LiveCaptionControllerFactory::GetInstance()->SetTestingFactory(
        web_contents()->GetBrowserContext(),
        base::BindOnce(
            [](std::unique_ptr<captions::LiveCaptionController> controller,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::move(controller);
            },
            std::move(controller)));
  }

  void TearDown() override {
    live_caption_controller_ = nullptr;
    pref_registry_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // ChromeRenderViewHostTestHarness
  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    pref_registry_ = pref_service->registry();
    RegisterUserProfilePrefs(pref_registry_);
    return TestingProfile::Builder()
        .SetPrefService(std::move(pref_service))
        .AddTestingFactories(GetTestingFactories())
        .Build();
  }

  // Get the MediaIntegration instance, after doing some work to register prefs
  GlicMediaIntegration* GetIntegration() {
    scoped_feature_list_.emplace(media::kHeadlessLiveCaption);
    return GlicMediaIntegration::GetFor(web_contents());
  }

  captions::LiveCaptionController* live_caption_controller() {
    return live_caption_controller_;
  }

  PrefService* pref_service() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
        ->GetPrefs();
  }

  bool get_headless_pref() {
    return pref_service()->GetBoolean(prefs::kHeadlessCaptionEnabled);
  }

  std::unique_ptr<captions::LiveCaptionController>
  CreateLiveCaptionController() {
    return std::make_unique<captions::LiveCaptionController>(
        pref_service(),
        /*global_prefs=*/nullptr, "application_locale", browser_context(),
        /*delegate=*/nullptr);
  }

 private:
  TestingPrefServiceSimple pref_service_;
  std::optional<base::test::ScopedFeatureList> scoped_feature_list_;
  raw_ptr<captions::LiveCaptionController> live_caption_controller_ = nullptr;
  raw_ptr<user_prefs::PrefRegistrySyncable> pref_registry_ = nullptr;
};

TEST_F(GlicMediaIntegrationTest, GetWithNullReturnsNull) {
  // Make sure this doesn't crash.
  EXPECT_EQ(GlicMediaIntegration::GetFor(nullptr), nullptr);
}

TEST_F(GlicMediaIntegrationTest, GetReturnsNullIfSwitchIsOff) {
  EXPECT_EQ(GlicMediaIntegration::GetFor(web_contents()), nullptr);
}

TEST_F(GlicMediaIntegrationTest, GetReturnsNonNullIfSwitchIsOn) {
  // Right now, this doesn't depend on the headless pref, but likely it should.
  EXPECT_NE(GetIntegration(), nullptr);
}

TEST_F(GlicMediaIntegrationTest, ContextContainsTranscript) {
  auto* integration = GetIntegration();

  // Send the string in pieces, mixing final and non-final ones.
  // It would be nice if we could set the max content size for testing.
  const std::string test_cap_1("ABC");
  const std::string test_cap_2("DEF");
  const std::string test_cap_3("XYZ");  // Should be ignored in all cases.
  const std::string test_cap_4("GHIJ");
  live_caption_controller()->DispatchTranscription(
      web_contents(), nullptr,
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true));
  live_caption_controller()->DispatchTranscription(
      web_contents(), nullptr,
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/true));
  // Non-final captions should be ignored.
  live_caption_controller()->DispatchTranscription(
      web_contents(), nullptr,
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/false));
  // nullptr `web_contents` should be ignored.
  live_caption_controller()->DispatchTranscription(
      /*web_contents=*/nullptr, nullptr,
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true));
  live_caption_controller()->DispatchTranscription(
      web_contents(), nullptr,
      media::SpeechRecognitionResult(test_cap_4, /*is_final=*/true));

  // Expect a leaf node with the entire context.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContext(web_contents(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_TRUE(root_node.has_content_attributes());
  EXPECT_EQ(root_node.content_attributes().text_data().text_content(),
            "ABCDEFGHIJ");
}

TEST_F(GlicMediaIntegrationTest, ContextContainsNoTranscript) {
  auto* integration = GetIntegration();

  // Send no strings.

  // Expect a leaf node with any text.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContext(web_contents(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_TRUE(root_node.has_content_attributes());
  EXPECT_GT(root_node.content_attributes().text_data().text_content().length(),
            0u);
}

TEST_F(GlicMediaIntegrationTest, HeadlessPrefTurnsOnAndOff) {
  // Verify that the headless pref turns on with the integration, and turns
  // back off the next time Live Caption starts.  This is temporary behavior.
  EXPECT_FALSE(get_headless_pref());
  GetIntegration();
  EXPECT_TRUE(get_headless_pref());
  auto controller = CreateLiveCaptionController();
  EXPECT_FALSE(get_headless_pref());
}

TEST_F(GlicMediaIntegrationTest, NullWebContentsIsOkay) {
  // Make sure that cases where no WebContents is provided don't crash.  This
  // includes cases where there is no media context for the given contents.
  optimization_guide::proto::ContentNode root_node;
  GetIntegration()->AppendContext(/*web_contents=*/nullptr, &root_node);
  // As long as nothing bad happens, it's good.
}

}  // namespace glic
