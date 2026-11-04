// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/content/browser/media_transcript_provider.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/mojo_capability_control_test_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/test_helper.h"
#endif

using content::WebContents;

namespace glic {

// Glic media integration requires SODA which is currently not supported on
// non-x86 Linux.
#if !BUILDFLAG(IS_LINUX) || defined(ARCH_CPU_X86_FAMILY)

namespace {

class MockSpeechRecognitionBrowserObserver
    : public media::mojom::SpeechRecognitionBrowserObserver {
 public:
  explicit MockSpeechRecognitionBrowserObserver(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionBrowserObserver>
          receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockSpeechRecognitionBrowserObserver() override = default;

  MOCK_METHOD(void, SpeechRecognitionAvailabilityChanged, (bool), (override));
  MOCK_METHOD(void,
              SpeechRecognitionLanguageChanged,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              SpeechRecognitionMaskOffensiveWordsChanged,
              (bool),
              (override));

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver> receiver_;
};

class StubCaptionBubbleController : public captions::CaptionBubbleController {
 public:
  StubCaptionBubbleController() = default;
  ~StubCaptionBubbleController() override = default;

  bool OnTranscription(content::RenderFrameHost*,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult&) override {
    return true;
  }
  void OnAudioStreamEnd(content::RenderFrameHost*,
                        captions::CaptionBubbleContext*) override {}
  void OnError(captions::CaptionBubbleContext*,
               captions::CaptionBubbleErrorType,
               captions::OnErrorClickedCallback,
               captions::OnDoNotShowAgainClickedCallback) override {}
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost*,
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}
  void UpdateCaptionStyle(std::optional<ui::CaptionStyle>) override {}
  bool IsWidgetVisibleForTesting() override { return true; }
  bool IsGenericErrorMessageVisibleForTesting() override { return false; }
  std::string GetBubbleLabelTextForTesting() override { return ""; }
  void CloseActiveModelForTesting() override {}
};

class StubCaptionControllerDelegate
    : public captions::CaptionControllerBase::Delegate {
 public:
  StubCaptionControllerDelegate() = default;
  ~StubCaptionControllerDelegate() override = default;

  std::unique_ptr<captions::CaptionBubbleController>
  CreateCaptionBubbleController(
      captions::CaptionBubbleSettings*,
      const std::string&,
      std::unique_ptr<captions::TranslationViewWrapperBase>) override {
    return std::make_unique<StubCaptionBubbleController>();
  }
  void AddCaptionStyleObserver(ui::NativeThemeObserver*) override {}
  void RemoveCaptionStyleObserver(ui::NativeThemeObserver*) override {}
};

}  // namespace

class GlicMediaIntegrationTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    // This must occur before base class SetUp() to ensure that the
    // TestingProfileManager is available when the profile is created,
    // allowing GlicKeyedServiceFactory to find it.
    profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        profile_manager_->profile_manager());
#endif
    ChromeRenderViewHostTestHarness::SetUp();
    glic_test_env_.SetupProfile(profile());
  }

  void TearDown() override {
    live_caption_controller_ = nullptr;
    pref_registry_ = nullptr;
    DeleteContents();
    profile_manager_->DeleteAllTestingProfiles();
    ChromeRenderViewHostTestHarness::TearDown();
    profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif
  }

  // ChromeRenderViewHostTestHarness
  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    pref_registry_ = pref_service->registry();
    RegisterUserProfilePrefs(pref_registry_);
    speech::SodaInstaller::RegisterLocalStatePrefs(pref_registry_);

    TestingProfile::Builder builder;
    builder.SetPrefService(std::move(pref_service));
    builder.AddTestingFactories(GetTestingFactories());

#if BUILDFLAG(IS_CHROMEOS)
    // This is hacky, but appears to be the only way to get the necessary
    // profile state setup correctly on ChromeOS.
    // TODO(b/501476411): Find a cleaner way to do this.
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        TestingProfile::kDefaultProfileUserName, GaiaId("1234567890")));
    std::string hash =
        user_manager::TestHelper::GetFakeUsernameHash(account_id);
    // Construct the absolute directory path to match BrowserContextHelper
    // expectations.
    base::FilePath path =
        profile_manager_->profiles_dir().AppendASCII("u-" + hash);
    builder.SetPath(path);
#endif

    auto profile = builder.Build();

    // Set up soda Installer
    soda_installer_.NeverDownloadSodaForTesting();
    ON_CALL(soda_installer_, Init).WillByDefault(testing::Return());

    return profile;
  }

  // Get the MediaIntegration instance, after doing some work to register prefs
  GlicMediaIntegration* GetIntegration() {
    SetFreCompleted();
    return GetIntegrationWithoutFreConsent();
  }

  GlicMediaIntegration* GetIntegrationWithoutFreConsent() {
    EnableHeadlessCaptionFeature();
    // Make sure that we have installed our LiveCaptionController before this,
    // because the integration will try to fetch it.  The test might have done
    // this earlier, however, which is also fine.
    /*void*/ live_caption_controller();
    // Make sure there's a keyed service, else the FRE profile checks break.
    return GlicMediaIntegration::GetFor(web_contents());
  }

  void EnableHeadlessCaptionFeature() {
    if (scoped_feature_list_) {
      return;
    }
    std::vector<base::test::FeatureRef> enabled_features{
        media::kHeadlessLiveCaption};
#if BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
#endif
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitWithFeatures(enabled_features, {});
    GlicMediaIntegration::EnsurePeerConnectionTrackerInitialized(profile());
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

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }

  bool get_headless_pref() {
    return pref_service()->GetBoolean(::prefs::kHeadlessCaptionEnabled);
  }

  std::unique_ptr<captions::LiveCaptionController>
  CreateLiveCaptionController() {
    return std::make_unique<captions::LiveCaptionController>(
        pref_service(), pref_service(), "application_locale", browser_context(),
        std::make_unique<StubCaptionControllerDelegate>());
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

  void SetFreCompleted() {
    glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
  }

  speech::MockSodaInstaller& soda_installer() { return soda_installer_; }

  void InstallSodaForTesting() {
    soda_installer_.NotifySodaInstalledForTesting();
    soda_installer_.NotifySodaInstalledForTesting(speech::LanguageCode::kEnUs);
  }

  mojo::Remote<blink::mojom::PeerConnectionTrackerHost>
  BindPeerConnectionTrackerHost(content::RenderFrameHost* frame_host) {
    content::RenderFrameHostTester::For(frame_host)
        ->InitializeRenderFrameIfNeeded();
    mojo::Remote<blink::mojom::PeerConnectionTrackerHost> tracker_host;
    mojo_capability_helper_.GetInterface(
        frame_host, tracker_host.BindNewPipeAndPassReceiver());
    return tracker_host;
  }

  void AddPeerConnectionViaMojo(
      mojo::Remote<blink::mojom::PeerConnectionTrackerHost>& tracker_host,
      int lid) {
    auto info = blink::mojom::PeerConnectionInfo::New(
        lid, /*rtc_configuration=*/"{}", /*url=*/"https://meet.example.com/");
    tracker_host->AddPeerConnection(std::move(info));
    tracker_host.FlushForTesting();
  }

  std::unique_ptr<content::MediaStreamUI> SimulateMediaCapture(
      content::WebContents* target_web_contents,
      blink::mojom::MediaStreamType stream_type) {
    scoped_refptr<MediaStreamCaptureIndicator> capture_indicator =
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetMediaStreamCaptureIndicator();
    const blink::MediaStreamDevice device(stream_type, "id", "name");
    blink::mojom::StreamDevices devices;
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      devices.audio_device = device;
    } else {
      devices.video_device = device;
    }
    std::unique_ptr<content::MediaStreamUI> stream =
        capture_indicator->RegisterMediaStream(target_web_contents, devices);
    stream->OnStarted(base::DoNothing(),
                      content::MediaStreamUI::SourceCallback(), std::string(),
                      {}, content::MediaStreamUI::StateChangeCallback());
    EXPECT_TRUE(capture_indicator->IsCapturingUserMedia(target_web_contents));
    return stream;
  }

  TestingProfileManager* profile_manager() { return profile_manager_; }
  GlicUnitTestEnvironment& glic_test_env() { return glic_test_env_; }

 private:
  content::test::MojoCapabilityControlTestHelper mojo_capability_helper_;
  GlicUnitTestEnvironment glic_test_env_;
  raw_ptr<TestingProfileManager> profile_manager_ = nullptr;
  std::optional<base::test::ScopedFeatureList> scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<captions::LiveCaptionController> live_caption_controller_ = nullptr;
  raw_ptr<user_prefs::PrefRegistrySyncable> pref_registry_ = nullptr;
  speech::MockSodaInstaller soda_installer_;
};

TEST_F(GlicMediaIntegrationTest, GetWithNullReturnsNull) {
  // Make sure this doesn't crash.
  EXPECT_EQ(
      GlicMediaIntegration::GetFor(static_cast<content::WebContents*>(nullptr)),
      nullptr);
  EXPECT_EQ(GetMediaTranscriptProvider(), nullptr);
}

TEST_F(GlicMediaIntegrationTest, GetReturnsNullIfSwitchIsOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(media::kHeadlessLiveCaption);

  EXPECT_EQ(GlicMediaIntegration::GetFor(web_contents()), nullptr);
  EXPECT_EQ(GetMediaTranscriptProvider(), nullptr);

  // When headless captioning is unsupported,
  // EnsurePeerConnectionTrackerInitialized is a no-op and does not register the
  // global PeerConnectionTrackerHostObserver.
  GlicMediaIntegration::EnsurePeerConnectionTrackerInitialized(profile());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  auto tracker_host = BindPeerConnectionTrackerHost(rfh());
  AddPeerConnectionViaMojo(tracker_host, /*lid=*/1);
  EXPECT_FALSE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
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
    // AppendContext is a legacy path and no longer contains transcripts when
    // kAnnotatedPageContentWithMediaData is enabled.
    optimization_guide::proto::ContentNode root_node;
    integration->AppendContext(web_contents(), &root_node);
    EXPECT_EQ(root_node.children_nodes_size(), 0);
    if (base::FeatureList::IsEnabled(
            optimization_guide::features::kAnnotatedPageContentWithMediaData)) {
      EXPECT_FALSE(root_node.has_content_attributes());
    } else {
      EXPECT_TRUE(root_node.has_content_attributes());
      EXPECT_EQ(root_node.content_attributes().text_data().text_content(),
                "ABCDEFGHIJ");
    }
  }
}

TEST_F(GlicMediaIntegrationTest, ContextContainsNoTranscript) {
  auto* integration = GetIntegration();

  // Send no strings.

  // Expect no content attributes when there is no transcript.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_FALSE(root_node.has_content_attributes());
}

TEST_F(GlicMediaIntegrationTest, ContextTruncatesUTF8Correctly) {
  auto* integration = GetIntegration();

  // Create a 20002-byte string: one 4-byte character + 19998 'A's.
  // max_size_bytes_ is 20000. 20002 - 20000 = 2.
  // The truncation index falls in the middle of the 4-byte character.
  std::string test_cap = "𐍈";
  test_cap.append(19998, 'A');

  live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult(test_cap, /*is_final=*/true));

  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);

  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_TRUE(root_node.has_content_attributes());

  // The 4-byte character should be entirely removed to avoid invalid UTF-8.
  std::string result_text =
      root_node.content_attributes().text_data().text_content();
  EXPECT_TRUE(base::IsStringUTF8(result_text));
  EXPECT_EQ(result_text, std::string(19998, 'A'));
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

  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));

  // This should prevent the transcription from being recorded.
  integration->OnPeerConnectionAddedForTesting(rfh());

  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_TRUE(context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
  EXPECT_FALSE(live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));
}

TEST_F(GlicMediaIntegrationTest,
       PeerConnectionUpdatesSpeechRecognitionAvailability) {
  auto* integration = GetIntegration();
  soda_installer().NotifySodaInstalledForTesting();
  soda_installer().NotifySodaInstalledForTesting(speech::LanguageCode::kEnUs);

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  ASSERT_NE(client_browser_interface, nullptr);

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Adding a peer connection should notify the observer that speech
  // recognition is no longer available.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  integration->OnPeerConnectionAddedForTesting(rfh());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Removing the peer connection should notify the observer that speech
  // recognition is available again.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  integration->OnPeerConnectionRemovedForTesting(rfh());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
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
  EXPECT_TRUE(main_context->IsExcludedFromTranscript());
  EXPECT_TRUE(subframe_context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(main_frame));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(subframe));

  // Remove the peer connection.
  integration->OnPeerConnectionRemovedForTesting(main_frame);

  // Verify both frames are no longer excluded.
  EXPECT_FALSE(main_context->IsExcludedFromTranscript());
  EXPECT_FALSE(subframe_context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(main_frame));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(subframe));
}

TEST_F(GlicMediaIntegrationTest,
       PeerConnectionInDocumentPiPPreventsTranscription) {
  auto* integration = GetIntegration();
  auto* main_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Create a document pip window.
  std::unique_ptr<content::WebContents> pip_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr);
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  pip_window_manager->EnterDocumentPictureInPicture(web_contents(),
                                                    pip_web_contents.get());

  // Add a peer connection to the pip window.
  integration->OnPeerConnectionAddedForTesting(
      pip_web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(main_context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(
      pip_web_contents->GetPrimaryMainFrame()));

  // Remove the peer connection.
  integration->OnPeerConnectionRemovedForTesting(
      pip_web_contents->GetPrimaryMainFrame());
  EXPECT_FALSE(main_context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(
      pip_web_contents->GetPrimaryMainFrame()));
}

TEST_F(GlicMediaIntegrationTest, ExcludedOriginsStopTranscription) {
  // Sending a transcript to an excluded origin should request that
  // transcription stops.
  auto* integration = GetIntegration();
  const url::Origin excluded_origin =
      url::Origin::Create(GURL("https://excluded.com"));
  SetCommittedOriginOnAllFrames(excluded_origin);

  // Verify that transcriptions are allowed initially.
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(
      web_contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      web_contents()->GetPrimaryMainFrame(), nullptr,
      media::SpeechRecognitionResult("some transcript", /*is_final=*/true)));

  // Setting the excluded origin list to include our origin should cause them to
  // start being ignored.
  integration->SetExcludedOrigins({excluded_origin});
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(
      web_contents()->GetPrimaryMainFrame()));
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

  // Expect no content attributes when the origin is excluded.
  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_FALSE(root_node.has_content_attributes());
}

TEST_F(GlicMediaIntegrationTest, DefaultExcludedOriginsStopTranscription) {
  // Get the integration, which will set the default excluded origins.
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  // Set the origin to youtube.
  const url::Origin youtube_origin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  SetCommittedOriginOnAllFrames(youtube_origin);

  // Transcription should not be allowed and dispatching a transcription should
  // be stopped.
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(
      web_contents()->GetPrimaryMainFrame()));
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

  // Transcription should not be allowed and dispatching a transcription should
  // be stopped.
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(
      web_contents()->GetPrimaryMainFrame()));
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

  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(
      web_contents()->GetPrimaryMainFrame()));
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
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_EQ(root_node.children_nodes_size(), 0);
  EXPECT_FALSE(root_node.has_content_attributes());
}

TEST_F(GlicMediaIntegrationTest, PrefToggleAddsAndRemovesListener) {
  // Ensure the pref is enabled initially.
  pref_service()->SetBoolean(glic::prefs::kGlicMediaUnderstandingEnabled, true);

  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);

  const url::Origin other_origin =
      url::Origin::Create(GURL("https://example.com"));
  SetCommittedOriginOnAllFrames(other_origin);

  // 1. With pref enabled, dispatching transcription should succeed.
  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult("transcript 1", /*is_final=*/true)));
  EXPECT_EQ(GetContext()->GetTranscriptChunks().size(), 1u);

  // 2. Disable the pref.
  pref_service()->SetBoolean(glic::prefs::kGlicMediaUnderstandingEnabled,
                             false);

  // Wait for the asynchronous removal to process.
  task_environment()->RunUntilIdle();

  // 3. Now the listener should be removed, so DispatchTranscription returns
  // false.
  EXPECT_FALSE(live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult("transcript 2", /*is_final=*/true)));

  // 4. Disabling the pref should have cleared the transcripts.
  ASSERT_NE(GetContext(), nullptr);
  EXPECT_FALSE(GetContext()->HasTranscriptChunks());
  EXPECT_EQ(GetContext()->GetTranscriptChunks().size(), 0u);

  // 5. Enable the pref again.
  pref_service()->SetBoolean(glic::prefs::kGlicMediaUnderstandingEnabled, true);

  // The listener is added back synchronously.
  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      rfh(), nullptr,
      media::SpeechRecognitionResult("transcript 3", /*is_final=*/true)));
  EXPECT_EQ(GetContext()->GetTranscriptChunks().size(), 1u);
}

TEST_F(GlicMediaIntegrationTest,
       PeerConnectionCreatedBeforeLazyInitPreventsTranscription) {
  EnableHeadlessCaptionFeature();
  ASSERT_FALSE(base::FeatureList::IsEnabled(media::kHeadlessCaptionEarlyStart));

  // Simulate a profile where GlicKeyedService has started at profile creation,
  // but the user has not yet completed FRE consent.
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kNotStarted);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  // Add a real WebRTC peer connection via Mojo BEFORE GlicMediaIntegration is
  // created or FRE consent is granted.
  auto tracker_host = BindPeerConnectionTrackerHost(rfh());
  AddPeerConnectionViaMojo(tracker_host, /*lid=*/1);

  auto* integration = GetIntegrationWithoutFreConsent();
  ASSERT_NE(integration, nullptr);
  EXPECT_FALSE(integration->IsInitializedForTesting());

  InstallSodaForTesting();
  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  ASSERT_NE(client_browser_interface, nullptr);

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());

  // Before FRE consent, headless captioning is not enabled.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Completing FRE initializes GlicMediaIntegration and enables
  // kHeadlessCaptionEnabled, but availability must remain false because of the
  // pre-existing peer connection.
  EXPECT_CALL(observer, SpeechRecognitionLanguageChanged(testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(testing::AtLeast(1));
  SetFreCompleted();
  EXPECT_TRUE(integration->IsInitializedForTesting());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_TRUE(context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));

  // Removing the pre-existing peer connection un-excludes the tab and notifies
  // the observer that transcription is now available.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  tracker_host->RemovePeerConnection(/*lid=*/1);
  tracker_host.FlushForTesting();
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
}

TEST_F(GlicMediaIntegrationTest,
       PeerConnectionCreatedBeforeGetIntegrationPreventsTranscription) {
  EnableHeadlessCaptionFeature();
  ASSERT_FALSE(base::FeatureList::IsEnabled(media::kHeadlessCaptionEarlyStart));

  // Ensure GlicKeyedService is instantiated (as
  // ServiceIsCreatedWithBrowserContext does on startup), while
  // GlicMediaIntegration::GetFor() has NOT been called.
  ASSERT_NE(glic::GlicKeyedService::Get(profile()), nullptr);
  EXPECT_FALSE(get_headless_pref());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  // Start a peer connection before GlicMediaIntegration::GetFor() is ever
  // called on this profile.
  auto tracker_host = BindPeerConnectionTrackerHost(rfh());
  AddPeerConnectionViaMojo(tracker_host, /*lid=*/7);

  InstallSodaForTesting();
  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Lazily creating GlicMediaIntegration flips kHeadlessCaptionEnabled true,
  // but availability must stay false while the pre-existing call is active.
  EXPECT_CALL(observer, SpeechRecognitionLanguageChanged(testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(testing::AtLeast(1));
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  EXPECT_TRUE(integration->IsInitializedForTesting());
  EXPECT_TRUE(get_headless_pref());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  tracker_host->RemovePeerConnection(/*lid=*/7);
  tracker_host.FlushForTesting();
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
}

TEST_F(GlicMediaIntegrationTest,
       DynamicSubframesAndNavigationsAfterPeerConnectionAdded) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  content::RenderFrameHost* main_frame = rfh();

  // Add a peer connection on the main frame BEFORE creating any subframe.
  auto main_tracker = BindPeerConnectionTrackerHost(main_frame);
  AddPeerConnectionViaMojo(main_tracker, /*lid=*/1);

  // Dynamically append and navigate a subframe after OnPeerConnectionAdded.
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub.example.com/a"),
          content::RenderFrameHostTester::For(main_frame)
              ->AppendChild("dynamic_subframe"));
  auto* subframe_context =
      GlicMediaContext::GetOrCreateForCurrentDocument(subframe);
  EXPECT_TRUE(subframe_context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(subframe));

  // Navigating the subframe to a new document keeps the new document excluded
  // while the main frame's peer connection remains active.
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://sub.example.org/b"), subframe);
  auto* navigated_subframe_context =
      GlicMediaContext::GetOrCreateForCurrentDocument(subframe);
  EXPECT_TRUE(navigated_subframe_context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(subframe));

  // Move the active peer connection onto the subframe, then navigate the
  // subframe cross-document without explicitly calling RemovePeerConnection.
  auto subframe_tracker = BindPeerConnectionTrackerHost(subframe);
  AddPeerConnectionViaMojo(subframe_tracker, /*lid=*/2);
  main_tracker->RemovePeerConnection(/*lid=*/1);
  main_tracker.FlushForTesting();
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(main_frame));

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> main_observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), main_frame->GetGlobalId());
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  // Navigating the subframe destroys its old DocumentUserData
  // (PeerConnectionTrackerHost), automatically clearing exclusion on the tab.
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other-sub.example.net/c"), subframe);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(main_frame));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(subframe));
}

TEST_F(GlicMediaIntegrationTest,
       SubframeDestroyedWithActivePeerConnectionUnexcludesSurvivingFrames) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  content::RenderFrameHost* main_frame = rfh();

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> main_observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), main_frame->GetGlobalId());
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  // 1. Live subframe detached while holding an active peer connection.
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub.example.com/call"),
          content::RenderFrameHostTester::For(main_frame)
              ->AppendChild("subframe"));
  const content::GlobalRenderFrameHostId subframe_id = subframe->GetGlobalId();

  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  auto subframe_tracker = BindPeerConnectionTrackerHost(subframe);
  AddPeerConnectionViaMojo(subframe_tracker, /*lid=*/42);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  auto* main_context =
      GlicMediaContext::GetOrCreateForCurrentDocument(main_frame);
  EXPECT_TRUE(main_context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(main_frame));

  // Detach the subframe while its peer connection is still active.
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  content::RenderFrameHostTester::For(subframe)->Detach();
  EXPECT_EQ(content::RenderFrameHost::FromID(subframe_id), nullptr);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  EXPECT_FALSE(main_context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(main_frame));

  // 2. Non-live RenderFrameHost destroyed while PeerConnectionTrackerHost still
  // holds an active peer connection: Detach() skips RenderFrameDeleted(), so
  // ~RenderFrameHostImpl erases subframe2_id from g_routing_id_frame_map
  // (FromID() == nullptr) BEFORE ~PeerConnectionTrackerHost invokes
  // OnPeerConnectionRemoved().
  content::RenderFrameHost* subframe2 =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://isolated-sub.other-domain.example/call"),
          content::RenderFrameHostTester::For(main_frame)
              ->AppendChild("subframe2"));
  const content::GlobalRenderFrameHostId subframe2_id =
      subframe2->GetGlobalId();
  auto subframe2_tracker = BindPeerConnectionTrackerHost(subframe2);
  static_cast<content::MockRenderProcessHost*>(subframe2->GetProcess())
      ->SimulateCrash();
  ASSERT_FALSE(subframe2->IsRenderFrameLive());

  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  AddPeerConnectionViaMojo(subframe2_tracker, /*lid=*/43);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);
  EXPECT_TRUE(main_context->IsExcludedFromTranscript());

  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  content::RenderFrameHostTester::For(subframe2)->Detach();
  EXPECT_EQ(content::RenderFrameHost::FromID(subframe2_id), nullptr);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  EXPECT_FALSE(main_context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(main_frame));
}

TEST_F(GlicMediaIntegrationTest,
       CrashedSubframeWithActivePeerConnectionUnexcludesSurvivingFrames) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  content::RenderFrameHost* main_frame = rfh();

  // Create and commit a cross-origin subframe with a live RenderFrame.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("oopif");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://isolated.other-site.example/call"), subframe);
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  ASSERT_TRUE(subframe->IsRenderFrameLive());
  const content::GlobalRenderFrameHostId subframe_id = subframe->GetGlobalId();

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> main_observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), main_frame->GetGlobalId());
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  auto subframe_tracker = BindPeerConnectionTrackerHost(subframe);
  AddPeerConnectionViaMojo(subframe_tracker, /*lid=*/99);
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  // Crash the subframe's renderer process. RenderFrameHostImpl stays alive
  // (along with document_associated_data_ / PeerConnectionTrackerHost), while
  // RenderFrameDeleted() fires. Note: if the test environment shares a process
  // with main_frame, both frames receive RenderFrameDeleted; if isolated, only
  // subframe crashes. Verify exclusion is cleared immediately in either case!
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(testing::AtLeast(1));
  auto* subframe_process =
      static_cast<content::MockRenderProcessHost*>(subframe->GetProcess());
  subframe_process->SimulateCrash();
  EXPECT_NE(content::RenderFrameHost::FromID(subframe_id), nullptr);
  EXPECT_FALSE(subframe->IsRenderFrameLive());
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);

  EXPECT_FALSE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(main_frame));

  // Detaching the crashed subframe later runs ~PeerConnectionTrackerHost and
  // must be a clean no-op with no duplicate notifications.
  EXPECT_CALL(main_observer, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  content::RenderFrameHostTester::For(subframe)->Detach();
  main_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&main_observer);
}

TEST_F(GlicMediaIntegrationTest,
       DocumentPipClosedWithActivePeerConnectionUnexcludesOpener) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  auto* main_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> opener_observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  opener_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);

  std::unique_ptr<content::WebContents> pip_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr);
  content::WebContentsTester::For(pip_web_contents.get())
      ->NavigateAndCommit(GURL("https://www.example.com/pip"));
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  pip_window_manager->EnterDocumentPictureInPicture(web_contents(),
                                                    pip_web_contents.get());
  opener_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);

  // Add a real peer connection inside the Document PiP window.
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  auto pip_tracker =
      BindPeerConnectionTrackerHost(pip_web_contents->GetPrimaryMainFrame());
  AddPeerConnectionViaMojo(pip_tracker, /*lid=*/5);
  opener_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  EXPECT_TRUE(main_context->IsExcludedFromTranscript());

  // Destroy the Document PiP WebContents directly without calling
  // RemovePeerConnection.
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(testing::AtLeast(1));
  pip_web_contents.reset();
  opener_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);

  EXPECT_FALSE(main_context->IsExcludedFromTranscript());
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));

  pip_window_manager->ExitPictureInPicture();
}

TEST_F(GlicMediaIntegrationTest, DocumentPipSymmetryAndUnrelatedTabIsolation) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  std::unique_ptr<content::WebContents> pip_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr);
  std::unique_ptr<content::WebContents> unrelated_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr);

  content::RenderFrameHost* opener_rfh = rfh();
  content::RenderFrameHost* pip_rfh = pip_web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* unrelated_rfh =
      unrelated_web_contents->GetPrimaryMainFrame();

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
      opener_remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> opener_observer(
      opener_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(opener_remote), opener_rfh->GetGlobalId());
  opener_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
      pip_remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> pip_observer(
      pip_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(pip_remote), pip_rfh->GetGlobalId());
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  // Start audio capture on the opener BEFORE entering Document PiP; entering
  // Document PiP must immediately notify the PiP child observer (false).
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  auto opener_capture = SimulateMediaCapture(
      web_contents(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  pip_window_manager->EnterDocumentPictureInPicture(web_contents(),
                                                    pip_web_contents.get());
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(pip_rfh));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(unrelated_rfh));

  // Stop capture on the opener; both opener and PiP child become available.
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  opener_capture.reset();
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  // An unrelated third tab opening a peer connection must NOT exclude either
  // the PiP opener or the PiP child.
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  integration->OnPeerConnectionAddedForTesting(unrelated_rfh);
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  EXPECT_FALSE(
      live_caption_controller()->IsTranscriptionAllowed(unrelated_rfh));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(opener_rfh));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(pip_rfh));

  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  integration->OnPeerConnectionRemovedForTesting(unrelated_rfh);
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  // Add a peer connection on the opener while PiP is open: both opener and PiP
  // child are excluded. When PiP exits first, PiP child un-excludes immediately
  // while opener stays excluded (not double-counted or prematurely cleared).
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  integration->OnPeerConnectionAddedForTesting(opener_rfh);
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  pip_window_manager->ExitPictureInPicture();
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);

  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(opener_rfh));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(pip_rfh));

  // Removing the opener's single peer connection now un-excludes the opener.
  EXPECT_CALL(opener_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  EXPECT_CALL(pip_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  integration->OnPeerConnectionRemovedForTesting(opener_rfh);
  opener_observer.FlushForTesting();
  pip_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&opener_observer);
  testing::Mock::VerifyAndClearExpectations(&pip_observer);
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(opener_rfh));
}

#if !BUILDFLAG(IS_CHROMEOS)
// ash::GlicUserSessionTestHelper installs a single-profile
// ScopedAccountIdAnnotator on ChromeOS.
TEST_F(GlicMediaIntegrationTest, MultiProfileIsolation) {
  auto* integration1 = GetIntegration();
  ASSERT_NE(integration1, nullptr);
  InstallSodaForTesting();

  TestingProfile* second_profile =
      profile_manager()->CreateTestingProfile("second_profile");
  ASSERT_NE(second_profile, nullptr);
  glic_test_env().SetupProfile(second_profile);

  captions::LiveCaptionControllerFactory::GetInstance()->SetTestingFactory(
      second_profile, base::BindOnce([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        Profile* prof = Profile::FromBrowserContext(context);
        return std::make_unique<captions::LiveCaptionController>(
            prof->GetPrefs(), prof->GetPrefs(), "application_locale", context,
            /*delegate=*/nullptr);
      }));
  captions::LiveCaptionControllerFactory::GetForProfile(second_profile);
  glic::GlicKeyedService::Get(second_profile)
      ->enabling()
      .SetCompletedFre(glic::prefs::FreStatus::kCompleted);

  std::unique_ptr<content::WebContents> second_web_contents =
      content::WebContentsTester::CreateTestWebContents(second_profile,
                                                        nullptr);
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  content::WebContentsTester::For(second_web_contents.get())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  auto* integration2 = GlicMediaIntegration::GetFor(second_web_contents.get());
  ASSERT_NE(integration2, nullptr);
  EXPECT_TRUE(integration2->IsInitializedForTesting());

  auto* client_interface1 =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  auto* client_interface2 =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
          second_profile);

  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote1;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer1(
      remote1.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_interface1->BindSpeechRecognitionBrowserObserver(std::move(remote1),
                                                          rfh()->GetGlobalId());
  observer1.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);

  content::RenderFrameHost* second_rfh =
      second_web_contents->GetPrimaryMainFrame();
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote2;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer2(
      remote2.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(observer2, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_interface2->BindSpeechRecognitionBrowserObserver(
      std::move(remote2), second_rfh->GetGlobalId());
  observer2.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer2);

  // Adding a real peer connection in profile 1 must NOT notify profile 2 or
  // double-count in profile 1.
  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  EXPECT_CALL(observer2, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  auto tracker1 = BindPeerConnectionTrackerHost(rfh());
  AddPeerConnectionViaMojo(tracker1, /*lid=*/1);
  observer1.FlushForTesting();
  observer2.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
  EXPECT_TRUE(
      captions::LiveCaptionControllerFactory::GetForProfile(second_profile)
          ->IsTranscriptionAllowed(second_rfh));

  // Removing that single peer connection immediately un-excludes profile 1.
  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  EXPECT_CALL(observer2, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  tracker1->RemovePeerConnection(/*lid=*/1);
  tracker1.FlushForTesting();
  observer1.FlushForTesting();
  observer2.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh()));

  // Capturing media in profile 1 must NOT notify profile 2.
  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  EXPECT_CALL(observer2, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  auto capture1 = SimulateMediaCapture(
      web_contents(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  observer1.FlushForTesting();
  observer2.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  capture1.reset();
  observer1.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);

  // Off-the-record child WebContents of profile 1 notifies its own
  // SpeechRecognitionClientBrowserInterface on capture without notifying
  // profile 1 or profile 2.
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  auto* otr_client_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
          otr_profile);
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
      otr_remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> otr_observer(
      otr_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(otr_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  otr_client_interface->BindSpeechRecognitionBrowserObserver(
      std::move(otr_remote),
      otr_web_contents->GetPrimaryMainFrame()->GetGlobalId());
  otr_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&otr_observer);

  EXPECT_CALL(otr_observer, SpeechRecognitionAvailabilityChanged(false))
      .Times(1);
  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  EXPECT_CALL(observer2, SpeechRecognitionAvailabilityChanged(testing::_))
      .Times(0);
  auto otr_capture =
      SimulateMediaCapture(otr_web_contents.get(),
                           blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  otr_observer.FlushForTesting();
  observer1.FlushForTesting();
  observer2.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&otr_observer);

  EXPECT_CALL(otr_observer, SpeechRecognitionAvailabilityChanged(true))
      .Times(1);
  otr_capture.reset();
  otr_observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&otr_observer);
  otr_web_contents.reset();
  profile()->DestroyOffTheRecordProfile(otr_profile);

  // Destroying the second profile must leave profile 1's peer connection
  // tracking intact.
  second_web_contents.reset();
  profile_manager()->DeleteTestingProfile("second_profile");

  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  AddPeerConnectionViaMojo(tracker1, /*lid=*/2);
  observer1.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);

  EXPECT_CALL(observer1, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  tracker1->RemovePeerConnection(/*lid=*/2);
  tracker1.FlushForTesting();
  observer1.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GlicMediaIntegrationTest,
       UserMediaAudioAndVideoCaptureUpdatesAvailabilityWithoutDeadlock) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  // Ensure rfh() has a committed document for DocumentService
  // (LiveCaptionSpeechRecognitionHost).
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  auto bind_recognition_host = [&]() {
    mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> host_remote;
    captions::LiveCaptionSpeechRecognitionHost::Create(
        rfh(), host_remote.BindNewPipeAndPassReceiver());
    return host_remote;
  };

  auto send_utterance =
      [](mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>& host,
         const std::string& text) {
        bool keep_recognizing = false;
        base::RunLoop loop;
        host->OnSpeechRecognitionRecognitionEvent(
            media::SpeechRecognitionResult(text, /*is_final=*/true),
            base::BindOnce(
                [](bool* out, base::RunLoop* run_loop, bool result) {
                  *out = result;
                  run_loop->Quit();
                },
                &keep_recognizing, &loop));
        loop.Run();
        return keep_recognizing;
      };

  // 1. Transcribe prior to media capture.
  auto host_remote = bind_recognition_host();
  EXPECT_TRUE(send_utterance(host_remote, "before "));

  // 2. Starting audio capture immediately notifies availability=false so the
  // renderer client resets its recognizer context instead of hitting sticky
  // stop_transcriptions_ = true.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  auto audio_stream = SimulateMediaCapture(
      web_contents(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
  host_remote.reset();

  // 3. Stopping audio capture immediately notifies availability=true and
  // resumes transcription cleanly on a fresh host.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  audio_stream.reset();
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  host_remote = bind_recognition_host();
  EXPECT_TRUE(send_utterance(host_remote, "middle "));

  // 4. Starting video capture also immediately notifies availability=false, and
  // stopping video capture notifies availability=true.
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  auto video_stream = SimulateMediaCapture(
      web_contents(), blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
  host_remote.reset();

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  video_stream.reset();
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  host_remote = bind_recognition_host();
  EXPECT_TRUE(send_utterance(host_remote, "after"));

  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_EQ(root_node.content_attributes().text_data().text_content(),
            "before middle after");
}

TEST_F(GlicMediaIntegrationTest,
       RegularLiveCaptionRemainsAllowedWhileBluefinDropsExcludedTranscripts) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  // Enable regular accessibility Live Caption.
  pref_service()->SetBoolean(::prefs::kLiveCaptionEnabled, true);

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), rfh()->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Even when a peer connection and user-media capture start, regular
  // accessibility Live Caption must remain available (true).
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(2);
  integration->OnPeerConnectionAddedForTesting(rfh());
  auto audio_stream = SimulateMediaCapture(
      web_contents(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Meanwhile, regular Live Caption keeps transcribing (DispatchTranscription
  // returns true via StubCaptionBubbleController) while Bluefin excludes and
  // drops transcripts for this frame.
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_TRUE(context->IsExcludedFromTranscript());
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh()));
  EXPECT_TRUE(
      context->OnResult(media::SpeechRecognitionResult("secret call", true)));
  EXPECT_TRUE(live_caption_controller()->DispatchTranscription(
      rfh(), nullptr, media::SpeechRecognitionResult("secret call", true)));
  EXPECT_FALSE(context->HasTranscriptChunks());

  optimization_guide::proto::ContentNode root_node;
  integration->AppendContextForFrame(rfh(), &root_node);
  EXPECT_FALSE(root_node.has_content_attributes());

  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(2);
  audio_stream.reset();
  integration->OnPeerConnectionRemovedForTesting(rfh());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(GlicMediaIntegrationTest,
       SameRenderFrameHostNavigationWithPeerConnectionDoesNotCrash) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  // Disable RenderDocument so same-origin subframe cross-document navigations
  // reuse the existing RenderFrameHostImpl and destroy its
  // DocumentAssociatedData (PeerConnectionTrackerHost) inside
  // SupportsUserData::ClearAllUserData().
  base::test::ScopedFeatureList disable_render_document;
  disable_render_document.InitAndDisableFeature(features::kRenderDocument);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://www.example.com/sub1"),
          content::RenderFrameHostTester::For(rfh())->AppendChild("subframe"));

  auto* client_browser_interface =
      SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile());
  mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver> remote;
  testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer(
      remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  client_browser_interface->BindSpeechRecognitionBrowserObserver(
      std::move(remote), subframe->GetGlobalId());
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Create GlicMediaContext and an active Mojo peer connection on `subframe`.
  GlicMediaContext::GetOrCreateForCurrentDocument(subframe);
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(false)).Times(1);
  auto tracker_host = BindPeerConnectionTrackerHost(subframe);
  AddPeerConnectionViaMojo(tracker_host, /*lid=*/1);
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Committing a cross-document navigation that reuses `subframe` runs
  // ~PeerConnectionTrackerHost() while SupportsUserData::in_clear_ is true,
  // which must cleanly notify availability=true without calling SetUserData().
  EXPECT_CALL(observer, SpeechRecognitionAvailabilityChanged(true)).Times(1);
  content::RenderFrameHost* navigated_subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://www.example.com/sub2"), subframe);
  ASSERT_EQ(navigated_subframe, subframe);
  observer.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(subframe));
}

TEST_F(GlicMediaIntegrationTest,
       InactiveFramesWithPeerConnectionDoNotExcludeActivePage) {
  auto* integration = GetIntegration();
  ASSERT_NE(integration, nullptr);
  InstallSodaForTesting();

  base::test::ScopedFeatureList bfcache_features;
  bfcache_features.InitWithFeaturesAndParameters(
      content::GetBasicBackForwardCacheFeatureForTesting(),
      content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());

  // 1. BackForwardCache transition (kActive -> kInBackForwardCache -> kActive):
  // An unconnected RTCPeerConnection on page A enters BFCache when navigating
  // to page B; page B must be allowed, and navigating back to page A must
  // re-exclude page A.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://site-a.example/"), rfh());
  content::RenderFrameHost* rfh_a = rfh();
  auto tracker_a = BindPeerConnectionTrackerHost(rfh_a);
  AddPeerConnectionViaMojo(tracker_a, /*lid=*/1);
  EXPECT_TRUE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh_a));

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://site-b.example/"), rfh_a);
  content::RenderFrameHost* rfh_b = rfh();
  ASSERT_NE(rfh_b, rfh_a);
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_FALSE(rfh_a->IsActive());

  // Page B is active and must NOT be excluded by BFCached page A.
  EXPECT_FALSE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh_a));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh_b));

  {
    auto* client_browser_interface =
        SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
            profile());
    mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
        remote_b;
    testing::StrictMock<MockSpeechRecognitionBrowserObserver> observer_b(
        remote_b.InitWithNewPipeAndPassReceiver());
    EXPECT_CALL(observer_b, SpeechRecognitionAvailabilityChanged(true))
        .Times(1);
    client_browser_interface->BindSpeechRecognitionBrowserObserver(
        std::move(remote_b), rfh_b->GetGlobalId());
    observer_b.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&observer_b);

    // Restoring page A from BFCache reactivates its peer connection exclusion.
    EXPECT_CALL(observer_b, SpeechRecognitionAvailabilityChanged(false))
        .Times(1);
    content::NavigationSimulator::GoBack(web_contents());
    ASSERT_EQ(rfh(), rfh_a);
    EXPECT_TRUE(rfh_a->IsActive());
    observer_b.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&observer_b);
  }
  EXPECT_TRUE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh_a));

  // 2. Pending-deletion transition (kActive -> kPendingDeletion):
  // Disable BFCache so navigating away leaves rfh_a in kPendingDeletion; the
  // newly committed page C must immediately be un-excluded.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  content::LeaveInPendingDeletionState(rfh_a);
  const content::GlobalRenderFrameHostId rfh_a_id = rfh_a->GetGlobalId();

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://site-c.example/"));
  content::RenderFrameHost* rfh_c = rfh();
  ASSERT_NE(rfh_c, rfh_a);
  EXPECT_NE(content::RenderFrameHost::FromID(rfh_a_id), nullptr);
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPendingDeletion);
  EXPECT_FALSE(rfh_a->IsActive());

  // Late AddPeerConnection IPC on a kPendingDeletion frame is also ignored.
  AddPeerConnectionViaMojo(tracker_a, /*lid=*/2);

  EXPECT_FALSE(GlicMediaIntegration::HasActivePeerConnection(web_contents()));
  EXPECT_FALSE(live_caption_controller()->IsTranscriptionAllowed(rfh_a));
  EXPECT_TRUE(live_caption_controller()->IsTranscriptionAllowed(rfh_c));
}

#endif  // !BUILDFLAG(IS_LINUX) || defined(ARCH_CPU_X86_FAMILY)

}  // namespace glic
