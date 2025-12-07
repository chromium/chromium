// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/content/browser/media_transcript_provider.h"
#include "components/soda/mock_soda_installer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

using content::WebContents;

namespace glic {

class GlicMediaIntegrationTest : public ChromeRenderViewHostTestHarness {
 public:
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

    auto profile = TestingProfile::Builder()
                       .SetPrefService(std::move(pref_service))
                       .AddTestingFactories(GetTestingFactories())
                       .Build();

    // Set up soda Installer
    soda_installer_.NeverDownloadSodaForTesting();
    ON_CALL(soda_installer_, Init).WillByDefault(testing::Return());

    return profile;
  }

  // Get the MediaIntegration instance, after doing some work to register prefs
  GlicMediaIntegration* GetIntegration() {
    std::vector<base::test::FeatureRef> enabled_features{
        media::kHeadlessLiveCaption};
#if BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
    // Make sure that we have installed our LiveCaptionController before this,
    // because the integration will try to fetch it.  The test might have done
    // this earlier, however, which is also fine.
    /*void*/ live_caption_controller();
    return GlicMediaIntegration::GetFor(web_contents());
  }

  optimization_guide::MediaTranscriptProvider* GetMediaTranscriptProvider() {
    return optimization_guide::MediaTranscriptProvider::GetFor(web_contents());
  }

  GlicMediaContext* GetContext() {
    return GlicMediaContext::GetForCurrentDocument(
        web_contents()->GetPrimaryMainFrame());
  }

  captions::LiveCaptionController* live_caption_controller() {
    if (live_caption_controller_) {
      return live_caption_controller_;
    }

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

  content::RenderFrameHost* rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void SetCommittedOriginOnAllFrames(const url::Origin& excluded_origin) {
    web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&excluded_origin](content::RenderFrameHost* rfh) {
          content::OverrideLastCommittedOrigin(rfh, excluded_origin);
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<captions::LiveCaptionController> live_caption_controller_ = nullptr;
  raw_ptr<user_prefs::PrefRegistrySyncable> pref_registry_ = nullptr;
  speech::MockSodaInstaller soda_installer_;
};

TEST_F(GlicMediaIntegrationTest, GetWithNullReturnsNull) {
  // Make sure this doesn't crash.
  EXPECT_EQ(GlicMediaIntegration::GetFor(nullptr), nullptr);
  EXPECT_EQ(GetMediaTranscriptProvider(), nullptr);
}

TEST_F(GlicMediaIntegrationTest, GetReturnsNullIfSwitchIsOff) {
  EXPECT_EQ(GlicMediaIntegration::GetFor(web_contents()), nullptr);
  EXPECT_EQ(GetMediaTranscriptProvider(), nullptr);
}

TEST_F(GlicMediaIntegrationTest, GetReturnsNonNullIfSwitchIsOn) {
  // This does not exist if integration is not created yet.
  EXPECT_EQ(GetMediaTranscriptProvider(), nullptr);
  // Right now, this doesn't depend on the headless pref, but likely it should.
  EXPECT_NE(GetIntegration(), nullptr);
  EXPECT_NE(GetMediaTranscriptProvider(), nullptr);
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
      rfh(), nullptr,
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true));
  live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/true));
  // Non-final captions should be ignored.
  live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/false));
  // nullptr `rfh` should be ignored.
  live_caption_controller()->DispatchTranscription(
      /*rfh=*/nullptr, nullptr,
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true));
  live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult(test_cap_4, /*is_final=*/true));

  {
    // Expect a leaf node with the entire context.
    optimization_guide::proto::ContentNode root_node;
    integration->AppendContextForFrame(rfh(), &root_node);
    EXPECT_EQ(root_node.children_nodes_size(), 0);
    EXPECT_TRUE(root_node.has_content_attributes());
    EXPECT_EQ(root_node.content_attributes().text_data().text_content(),
              "ABCDEFGHIJ");
  }

  {
    // Expect a leaf node with the entire context when we query with the
    // WebContents instead.
    optimization_guide::proto::ContentNode root_node;
    integration->AppendContext(web_contents(), &root_node);
    EXPECT_EQ(root_node.children_nodes_size(), 0);
    EXPECT_TRUE(root_node.has_content_attributes());
    EXPECT_EQ(root_node.content_attributes().text_data().text_content(),
              "ABCDEFGHIJ");
  }
}

TEST_F(GlicMediaIntegrationTest, ContextContainsNoTranscript) {
  auto* integration = GetIntegration();

  // Send no strings.

  // Expect a leaf node with no text.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_TRUE(root_node.has_content_attributes());
  EXPECT_EQ(root_node.content_attributes().text_data().text_content().length(),
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

TEST_F(GlicMediaIntegrationTest, NullRenderFrameHostIsOkay) {
  // Make sure that cases where no RFH is provided don't crash.  This
  // includes cases where there is no media context for the given contents.
  optimization_guide::proto::ContentNode root_node;
  GetIntegration()->AppendContextForFrame(/*rfh=*/nullptr, &root_node);
  // As long as nothing bad happens, it's good.
}

TEST_F(GlicMediaIntegrationTest, PeerConnectionPreventsTranscription) {
  auto* integration = GetIntegration();

  // This should prevent the transcription from being recorded.
  integration->OnPeerConnectionAddedForTesting(rfh());

  auto* context = GetContext();
  EXPECT_TRUE(context->is_excluded_from_transcript_for_testing());
}

TEST_F(GlicMediaIntegrationTest, PeerConnectionExcludesAllSubframes) {
  auto* integration = GetIntegration();
  auto* main_frame = rfh();
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  const GURL subframe_url("https://www.subframe.com/");
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          subframe_url, content::RenderFrameHostTester::For(main_frame)
                            ->AppendChild("subframe"));

  // Create contexts for both frames.
  auto* main_context =
      GlicMediaContext::GetOrCreateForCurrentDocument(main_frame);
  auto* subframe_context =
      GlicMediaContext::GetOrCreateForCurrentDocument(subframe);

  // Add a peer connection to the main frame.
  integration->OnPeerConnectionAddedForTesting(main_frame);

  // Verify both frames are excluded.
  EXPECT_TRUE(main_context->is_excluded_from_transcript_for_testing());
  EXPECT_TRUE(subframe_context->is_excluded_from_transcript_for_testing());

  // Remove the peer connection.
  integration->OnPeerConnectionRemovedForTesting(main_frame);

  // Verify both frames are no longer excluded.
  EXPECT_FALSE(main_context->is_excluded_from_transcript_for_testing());
  EXPECT_FALSE(subframe_context->is_excluded_from_transcript_for_testing());
}

TEST_F(GlicMediaIntegrationTest, ExcludedOriginsStopTranscription) {
  // Sending a transcript to an excluded origin should request that
  // transcription stops.
  auto* integration = GetIntegration();
  const url::Origin excluded_origin =
      url::Origin::Create(GURL("https://excluded.com"));
  SetCommittedOriginOnAllFrames(excluded_origin);

  // Verify that transcriptions are allowed initially.
  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));

  // Setting the excluded origin list to include our origin should cause them to
  // start being ignored.
  integration->SetExcludedOrigins({excluded_origin});
  EXPECT_FALSE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some other transcript",
                                     /*is_final=*/true)));
}

TEST_F(GlicMediaIntegrationTest, ExcludedOriginsDontReturnTranscriptions) {
  // Asking for context from an excluded origin should return nothing.
  auto* integration = GetIntegration();
  const url::Origin excluded_origin =
      url::Origin::Create(GURL("https://excluded.com"));
  SetCommittedOriginOnAllFrames(excluded_origin);
  ASSERT_TRUE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));

  // Exclude the origin after adding the transcript.
  integration->SetExcludedOrigins({excluded_origin});

  // Expect an empty transcript.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContext(web_contents(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_TRUE(root_node.has_content_attributes());
  EXPECT_EQ(root_node.content_attributes().text_data().text_content(), "");
}

TEST_F(GlicMediaIntegrationTest, DefaultExcludedOriginsStopTranscription) {
  // Get the integration, which will set the default excluded origins.
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  // Set the origin to youtube.
  const url::Origin youtube_origin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  SetCommittedOriginOnAllFrames(youtube_origin);

  // Dispatching a transcription should be stopped.
  EXPECT_FALSE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));
}

TEST_F(GlicMediaIntegrationTest, DefaultExcludedHttpOriginsStopTranscription) {
  // Get the integration, which will set the default excluded origins.
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  // Set the origin to youtube.
  const url::Origin youtube_origin =
      url::Origin::Create(GURL("http://www.youtube.com"));
  SetCommittedOriginOnAllFrames(youtube_origin);

  // Dispatching a transcription should be stopped.
  EXPECT_FALSE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));
}

TEST_F(GlicMediaIntegrationTest, NonExcludedOriginAllowsTranscription) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  const url::Origin other_origin =
      url::Origin::Create(GURL("https://example.com"));
  SetCommittedOriginOnAllFrames(other_origin);

  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));
}

TEST_F(GlicMediaIntegrationTest,
       DefaultExcludedOriginsDontReturnTranscriptions) {
  // Asking for context from a default excluded origin should return nothing.
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  // Set the origin to youtube.
  const url::Origin youtube_origin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  SetCommittedOriginOnAllFrames(youtube_origin);

  // This transcription should be ignored.
  live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true));

  // Expect an empty transcript.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContext(web_contents(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_FALSE(root_node.has_content_attributes());
}

}  // namespace glic
