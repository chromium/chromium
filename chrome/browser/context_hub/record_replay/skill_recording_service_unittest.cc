// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "ui/base/test/mock_base_window.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#else
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#endif

namespace context_hub {
namespace {

using ::base::test::TestFuture;
using browser::context_hub::mojom::SkillRecordingActionType;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::WithArg;

page_content_annotations::FetchPageContextResultCallbackArg
CreateSuccessfulScreenshotResult() {
  auto fetch_result =
      std::make_unique<page_content_annotations::FetchPageContextResult>();
  fetch_result->screenshot_result =
      page_content_annotations::ScreenshotResult(/*dimensions=*/{});
  fetch_result->screenshot_result->screenshot_data = {1, 2, 3, 4};
  fetch_result->screenshot_result->mime_type = "image/jpeg";
  return base::ok(std::move(fetch_result));
}

page_content_annotations::FetchPageContextResultCallbackArg
CreateFailedFetchResult() {
  return base::unexpected(
      page_content_annotations::FetchPageContextErrorDetails{
          page_content_annotations::FetchPageContextError::kUnknown,
          "Failed to fetch page context"});
}

optimization_guide::OptimizationGuideModelExecutionResult
CreateSkillGenerationMesResult(std::string_view skill_instructions) {
  optimization_guide::proto::ContextHubResponse response;
  response.mutable_memory_bank_chat_response()->set_text_response(
      skill_instructions);
  return optimization_guide::OptimizationGuideModelExecutionResult(
      base::ok(optimization_guide::AnyWrapProto(response)), nullptr);
}

class TestBrowserInstance {
 public:
  explicit TestBrowserInstance(Profile* profile)
      : profile_(CHECK_DEREF(profile)),
        tab_strip_model_(&tab_strip_model_delegate_, profile) {
    ON_CALL(mock_bwi_, GetProfile())
        .WillByDefault(testing::Return(&profile_.get()));
    ON_CALL(mock_bwi_, GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));
    ON_CALL(mock_bwi_, GetWindow())
        .WillByDefault(testing::Return(&mock_window_));
    ON_CALL(mock_bwi_, RegisterDidBecomeActive(testing::_))
        .WillByDefault(
            [this](BrowserWindowInterface::DidBecomeActiveCallback cb) {
              return active_callbacks_.Add(std::move(cb));
            });
    ON_CALL(mock_bwi_, RegisterBrowserDidClose(testing::_))
        .WillByDefault(
            [this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
              return close_callbacks_.Add(std::move(cb));
            });

    BrowserManagerServiceFactory::GetForProfile(&profile_.get())
        ->AddBrowserForTesting(&mock_bwi_);
  }

  TestBrowserInstance(const TestBrowserInstance&) = delete;
  TestBrowserInstance& operator=(const TestBrowserInstance&) = delete;

  ~TestBrowserInstance() {
    tab_strip_model_.CloseAllTabs();
    close_callbacks_.Notify(&mock_bwi_);
  }

  TabStripModel* tab_strip_model() { return &tab_strip_model_; }
  BrowserWindowInterface* bwi() { return &mock_bwi_; }

  void Activate() { active_callbacks_.Notify(&mock_bwi_); }

 private:
  const raw_ref<Profile> profile_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_;
  testing::NiceMock<ui::MockBaseWindow> mock_window_;
  testing::NiceMock<MockBrowserWindowInterface> mock_bwi_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> active_callbacks_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> close_callbacks_;
};

class TestSkillRecordingServiceObserver
    : public SkillRecordingService::Observer {
 public:
  TestSkillRecordingServiceObserver() = default;
  TestSkillRecordingServiceObserver(const TestSkillRecordingServiceObserver&) =
      delete;
  TestSkillRecordingServiceObserver& operator=(
      const TestSkillRecordingServiceObserver&) = delete;
  ~TestSkillRecordingServiceObserver() override = default;

  // SkillRecordingService::Observer:
  void OnTranscriptUpdate(const std::string& text,
                          base::TimeDelta timestamp,
                          bool is_final) override {
    last_text_ = text;
    last_timestamp_ = timestamp;
    last_is_final_ = is_final;
    update_count_++;
  }

  const std::string& last_text() const { return last_text_; }
  base::TimeDelta last_timestamp() const { return last_timestamp_; }
  bool last_is_final() const { return last_is_final_; }
  int update_count() const { return update_count_; }

 private:
  std::string last_text_;
  base::TimeDelta last_timestamp_;
  bool last_is_final_ = false;
  int update_count_ = 0;
};

class SkillRecordingServiceTest : public testing::Test {
 public:
  SkillRecordingServiceTest() {
#if BUILDFLAG(IS_CHROMEOS)
    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactory(&profile_,
                            ProfileKeyedServiceFactory::TestingFactory());
#else
    SpeechRecognitionServiceFactory::GetInstanceForTest()->SetTestingFactory(
        &profile_, ProfileKeyedServiceFactory::TestingFactory());
#endif
  }
  SkillRecordingServiceTest(const SkillRecordingServiceTest&) = delete;
  SkillRecordingServiceTest& operator=(const SkillRecordingServiceTest&) =
      delete;
  ~SkillRecordingServiceTest() override = default;

 protected:
  content::WebContents* AddTab(TestBrowserInstance* browser_instance,
                               const GURL& url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            browser_instance->bwi()->GetProfile(),
            content::SiteInstance::Create(
                browser_instance->bwi()->GetProfile()));
    content::WebContents* raw_contents = contents.get();
    content::WebContentsTester::For(raw_contents)->NavigateAndCommit(url);
    browser_instance->tab_strip_model()->InsertWebContentsAt(
        browser_instance->tab_strip_model()->count(), std::move(contents),
        AddTabTypes::ADD_INHERIT_OPENER | AddTabTypes::ADD_ACTIVE);
    return raw_contents;
  }

  void SimulateMouseDown(content::WebContents* web_contents,
                         float x,
                         float y,
                         blink::WebPointerProperties::Button button =
                             blink::WebPointerProperties::Button::kLeft) {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown, {x, y}, {x, y}, button,
        /*click_count_param=*/1, blink::WebInputEvent::kNoModifiers,
        base::TimeTicks::Now());
    web_contents->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->SimulateUserInteraction(mouse_event);
  }

  void ExpectExecuteModel(Matcher<std::string> user_command_matcher,
                          std::string_view response_text) {
    std::string response_str(response_text);
    EXPECT_CALL(
        mock_model_executor_,
        ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub,
                     _, _, _))
        .WillOnce([user_command_matcher = std::move(user_command_matcher),
                   response_str = std::move(response_str)](
                      optimization_guide::ModelBasedCapabilityKey feature,
                      const google::protobuf::MessageLite& request_metadata,
                      const optimization_guide::ModelExecutionOptions& options,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultCallback
                              callback) mutable {
          const auto& request =
              static_cast<const optimization_guide::proto::ContextHubRequest&>(
                  request_metadata);
          EXPECT_THAT(request.user_command(), user_command_matcher);
          std::move(callback).Run(CreateSkillGenerationMesResult(response_str),
                                  nullptr);
        });
  }

  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> mock_model_executor_;
  SkillRecordingService service_{&profile_, &mock_model_executor_};
};

TEST_F(SkillRecordingServiceTest, InitialState) {
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
  EXPECT_FALSE(service_.is_recording());
}

TEST_F(SkillRecordingServiceTest, StartAndStopRecording) {
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), base::TimeTicks::Now());

  service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(SkillRecordingServiceTest, OnScreenshotCapturedWithValidResult) {
  service_.StartRecording();
  task_environment_.FastForwardBy(base::Milliseconds(150));

  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");
  service_.BindScreenshotCapturedCallbackForTesting(0).Run(
      CreateSuccessfulScreenshotResult());

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 1u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:10, Y:20");
  EXPECT_EQ(result.actions[0].timestamp, base::Milliseconds(150));
  EXPECT_TRUE(result.actions[0].current_url.is_empty());
  EXPECT_EQ(result.actions[0].screenshot_jpeg_bytes,
            std::string("\x01\x02\x03\x04", 4));

  // Starting a new recording session clears artifacts from the previous
  // session.
  service_.StartRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
  service_.StopRecording();
}

TEST_F(SkillRecordingServiceTest,
       OnScreenshotCapturedPreservesActionsWhenScreenshotMissingOrPending) {
  service_.StartRecording();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  // Action 0 is left with no `OnScreenshotCaptured()` call (pending at stop).
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");

  task_environment_.FastForwardBy(base::Milliseconds(250));
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:30, Y:40");

  // Missing `screenshot_result`, failed fetch, and out-of-bounds index.
  service_.BindScreenshotCapturedCallbackForTesting(1).Run(base::ok(
      std::make_unique<page_content_annotations::FetchPageContextResult>()));
  service_.BindScreenshotCapturedCallbackForTesting(1).Run(
      CreateFailedFetchResult());
  service_.BindScreenshotCapturedCallbackForTesting(99).Run(
      CreateSuccessfulScreenshotResult());

  // Empty `screenshot_data` or empty `mime_type` must be ignored.
  auto empty_data_result =
      std::make_unique<page_content_annotations::FetchPageContextResult>();
  empty_data_result->screenshot_result =
      page_content_annotations::ScreenshotResult(/*dimensions=*/{});
  empty_data_result->screenshot_result->mime_type = "image/jpeg";
  service_.BindScreenshotCapturedCallbackForTesting(1).Run(
      base::ok(std::move(empty_data_result)));

  auto empty_mime_result =
      std::make_unique<page_content_annotations::FetchPageContextResult>();
  empty_mime_result->screenshot_result =
      page_content_annotations::ScreenshotResult(/*dimensions=*/{});
  empty_mime_result->screenshot_result->screenshot_data = {1, 2, 3, 4};
  service_.BindScreenshotCapturedCallbackForTesting(1).Run(
      base::ok(std::move(empty_mime_result)));

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:10, Y:20");
  EXPECT_EQ(result.actions[0].timestamp, base::Milliseconds(100));
  EXPECT_TRUE(result.actions[0].screenshot_jpeg_bytes.empty());
  EXPECT_EQ(result.actions[1].description, "Mouse Clicked at X:30, Y:40");
  EXPECT_EQ(result.actions[1].timestamp, base::Milliseconds(350));
  EXPECT_TRUE(result.actions[1].screenshot_jpeg_bytes.empty());
}

TEST_F(SkillRecordingServiceTest,
       StopRecordingInvalidatesPendingScreenshotCallbacks) {
  service_.StartRecording();
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "First session action");
  page_content_annotations::FetchPageContextResultCallback stale_callback =
      service_.BindScreenshotCapturedCallbackForTesting(/*action_index=*/0);

  service_.StopRecording();
  ASSERT_EQ(service_.artifacts().actions.size(), 1u);
  EXPECT_TRUE(service_.artifacts().actions[0].screenshot_jpeg_bytes.empty());

  // Start a new recording session and capture a new action at index 0.
  service_.StartRecording();
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Second session action");

  // Running the stale callback from the first session must not attach its
  // screenshot to action 0 of the second session.
  std::move(stale_callback).Run(CreateSuccessfulScreenshotResult());

  service_.StopRecording();
  const RecordingArtifacts& second_result = service_.artifacts();
  ASSERT_EQ(second_result.actions.size(), 1u);
  EXPECT_EQ(second_result.actions[0].description, "Second session action");
  EXPECT_TRUE(second_result.actions[0].screenshot_jpeg_bytes.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresActionsWhenNotRecording) {
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");
  service_.BindScreenshotCapturedCallbackForTesting(0).Run(
      CreateSuccessfulScreenshotResult());

  service_.StartRecording();
  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(SkillRecordingServiceTest, CapsRecordedActionsAtMaxLimit) {
  service_.StartRecording();
  for (size_t i = 0; i < RecordingArtifacts::kMaxRecordedActions; ++i) {
    service_.CaptureAction(SkillRecordingActionType::kLeftClick, "Click");
  }
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Overflow click");

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), RecordingArtifacts::kMaxRecordedActions);
  EXPECT_EQ(result.actions.back().description, "Click");
}

TEST_F(SkillRecordingServiceTest, IgnoresMultipleStartRecording) {
  const base::TimeTicks initial_start_time = base::TimeTicks::Now();
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), initial_start_time);
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");

  task_environment_.FastForwardBy(base::Seconds(5));

  // Calling `StartRecording()` again while already recording should be a no-op
  // and not reset actions.
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  // The start time should be unchanged.
  EXPECT_EQ(service_.start_time(), initial_start_time);
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:30, Y:40");

  service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  ASSERT_EQ(service_.artifacts().actions.size(), 2u);
}

TEST_F(SkillRecordingServiceTest, ShouldTrackBrowser_MatchesSameProfileOnly) {
  TestBrowserInstance same_profile_browser(&profile_);
  EXPECT_TRUE(service_.ShouldTrackBrowser(same_profile_browser.bwi()));
  EXPECT_FALSE(service_.ShouldTrackBrowser(nullptr));

  Profile* otr_profile = profile_.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  TestBrowserInstance otr_browser(otr_profile);
  EXPECT_FALSE(service_.ShouldTrackBrowser(otr_browser.bwi()));

  TestingProfile other_profile;
  TestBrowserInstance other_browser(&other_profile);
  EXPECT_FALSE(service_.ShouldTrackBrowser(other_browser.bwi()));
}

TEST_F(SkillRecordingServiceTest, RecordsLeftMouseClicksOnHttpOrHttpsTab) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* tab =
      AddTab(&browser, GURL("https://example.com/page"));

  service_.StartRecording();
  EXPECT_EQ(
      page_content_annotations::PageContextFetcherManager::FromWebContents(tab),
      nullptr);
  task_environment_.FastForwardBy(base::Milliseconds(150));
  SimulateMouseDown(tab, 42.0f, 84.0f,
                    blink::WebPointerProperties::Button::kLeft);
  EXPECT_NE(
      page_content_annotations::PageContextFetcherManager::FromWebContents(tab),
      nullptr);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 1u);
  EXPECT_EQ(result.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:42, Y:84");
  EXPECT_EQ(result.actions[0].timestamp, base::Milliseconds(150));
  EXPECT_EQ(result.actions[0].current_url, GURL("https://example.com/page"));
}

TEST_F(SkillRecordingServiceTest, SanitizesRecordedUrls) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* tab = AddTab(
      &browser, GURL("https://user:pass@example.com/page?q=secret#section"));

  service_.StartRecording();
  SimulateMouseDown(tab, 10.0f, 20.0f);

  // Navigate to a URL whose path exceeds `kMaxRecordedUrlLength` (500 chars);
  // the path should be cleared while preserving the origin.
  content::WebContentsTester::For(tab)->NavigateAndCommit(
      GURL("https://example.com/" + std::string(500, 'a')));
  SimulateMouseDown(tab, 30.0f, 40.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].current_url, GURL("https://example.com/page"));
  EXPECT_EQ(result.actions[1].current_url, GURL("https://example.com/"));
}

TEST_F(SkillRecordingServiceTest, IgnoresNonLeftMouseButtonClicks) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* tab =
      AddTab(&browser, GURL("https://example.com/page"));

  service_.StartRecording();
  SimulateMouseDown(tab, 10.0f, 20.0f,
                    blink::WebPointerProperties::Button::kRight);
  SimulateMouseDown(tab, 30.0f, 40.0f,
                    blink::WebPointerProperties::Button::kMiddle);
  SimulateMouseDown(tab, 50.0f, 60.0f,
                    blink::WebPointerProperties::Button::kBack);
  SimulateMouseDown(tab, 70.0f, 80.0f,
                    blink::WebPointerProperties::Button::kForward);
  EXPECT_EQ(
      page_content_annotations::PageContextFetcherManager::FromWebContents(tab),
      nullptr);

  service_.StopRecording();
  EXPECT_TRUE(service_.artifacts().actions.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresClicksOnContextHubAndNonHttpUrls) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* web_tab =
      AddTab(&browser, GURL("https://example.com/page"));
  content::WebContents* context_hub_tab =
      AddTab(&browser, GURL(chrome::kChromeUIContextHubURL));

  service_.StartRecording();

  // Clicking on `chrome://context-hub` (e.g., "Stop Recording") must be
  // ignored.
  SimulateMouseDown(context_hub_tab, 100.0f, 200.0f);

  // Switching to the HTTPS tab and clicking should record.
  browser.tab_strip_model()->ActivateTabAt(0);
  SimulateMouseDown(web_tab, 55.0f, 65.0f);

  // Switching back to `chrome://context-hub` to stop recording and clicking
  // must not add a phantom click.
  browser.tab_strip_model()->ActivateTabAt(1);
  SimulateMouseDown(context_hub_tab, 100.0f, 200.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 1u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:55, Y:65");
}

TEST_F(SkillRecordingServiceTest,
       SwitchesObservedTabWhenBrowserWindowActivationChanges) {
  TestBrowserInstance window1(&profile_);
  content::WebContents* tab1 =
      AddTab(&window1, GURL("https://example.com/window1"));

  TestBrowserInstance window2(&profile_);
  content::WebContents* tab2 =
      AddTab(&window2, GURL("https://example.com/window2"));

  // Make `window2` the most recently active window before starting recording.
  window2.Activate();

  service_.StartRecording();

  // Initial observation should attach to `window2` (the active window), not
  // `window1` (the older inactive window).
  SimulateMouseDown(tab1, 11.0f, 22.0f);
  SimulateMouseDown(tab2, 33.0f, 44.0f);

  // Now switch window focus to `window1` without changing tabs inside
  // `window1`'s tab strip.
  window1.Activate();
  SimulateMouseDown(tab2, 55.0f, 66.0f);
  SimulateMouseDown(tab1, 77.0f, 88.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 2u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:33, Y:44");
  EXPECT_EQ(result.actions[0].current_url, GURL("https://example.com/window2"));
  EXPECT_EQ(result.actions[1].description, "Mouse Clicked at X:77, Y:88");
  EXPECT_EQ(result.actions[1].current_url, GURL("https://example.com/window1"));
}

TEST_F(SkillRecordingServiceTest, IgnoresClicksFromOtherProfiles) {
  TestBrowserInstance main_window(&profile_);
  content::WebContents* main_tab =
      AddTab(&main_window, GURL("https://example.com/main"));
  main_window.Activate();

  TestingProfile other_profile;
  TestBrowserInstance other_window(&other_profile);
  content::WebContents* other_tab =
      AddTab(&other_window, GURL("https://example.com/other"));
  other_window.Activate();

  service_.StartRecording();

  SimulateMouseDown(other_tab, 10.0f, 20.0f);
  SimulateMouseDown(main_tab, 30.0f, 40.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 1u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:30, Y:40");
}

TEST_F(SkillRecordingServiceTest,
       UpdatesObservedTabWhenActiveWebContentsIsReplaced) {
  TestBrowserInstance browser(&profile_);
  content::WebContents* original_tab =
      AddTab(&browser, GURL("https://example.com/original"));

  service_.StartRecording();

  std::unique_ptr<content::WebContents> replacement_contents =
      content::WebContentsTester::CreateTestWebContents(
          &profile_, content::SiteInstance::Create(&profile_));
  content::WebContents* raw_replacement = replacement_contents.get();
  content::WebContentsTester::For(raw_replacement)
      ->NavigateAndCommit(GURL("https://example.com/replacement"));

  std::unique_ptr<content::WebContents> old_contents =
      browser.tab_strip_model()->DiscardWebContents(
          original_tab, std::move(replacement_contents));

  SimulateMouseDown(old_contents.get(), 10.0f, 20.0f);
  SimulateMouseDown(raw_replacement, 50.0f, 60.0f);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.actions.size(), 1u);
  EXPECT_EQ(result.actions[0].description, "Mouse Clicked at X:50, Y:60");
  EXPECT_EQ(result.actions[0].current_url,
            GURL("https://example.com/replacement"));
}

TEST_F(SkillRecordingServiceTest, ForwardsTranscriptUpdatesToObservers) {
  TestSkillRecordingServiceObserver observer;
  service_.AddObserver(&observer);

  // Transcript updates while not recording are ignored.
  service_.OnTranscriptUpdateForTesting("ignored before start",
                                        base::Milliseconds(100),
                                        /*is_final=*/true);
  EXPECT_EQ(observer.update_count(), 0);
  EXPECT_TRUE(service_.artifacts().transcripts.empty());

  service_.StartRecording();

  // Partial transcripts are forwarded to observers but not stored in
  // `artifacts().transcripts`.
  service_.OnTranscriptUpdateForTesting("hello", base::Milliseconds(100),
                                        /*is_final=*/false);
  EXPECT_EQ(observer.update_count(), 1);
  EXPECT_EQ(observer.last_text(), "hello");
  EXPECT_EQ(observer.last_timestamp(), base::Milliseconds(100));
  EXPECT_FALSE(observer.last_is_final());
  EXPECT_TRUE(service_.artifacts().transcripts.empty());

  // Whitespace-only finalized transcripts are ignored when storing in
  // `artifacts().transcripts`.
  service_.OnTranscriptUpdateForTesting("   \t\n  ", base::Milliseconds(200),
                                        /*is_final=*/true);
  EXPECT_EQ(observer.update_count(), 2);
  EXPECT_TRUE(service_.artifacts().transcripts.empty());

  // Finalized transcripts are trimmed and stored in `artifacts().transcripts`,
  // and forwarded to observers.
  service_.OnTranscriptUpdateForTesting("  hello world \n",
                                        base::Milliseconds(250),
                                        /*is_final=*/true);
  EXPECT_EQ(observer.update_count(), 3);
  EXPECT_EQ(observer.last_text(), "  hello world \n");
  EXPECT_EQ(observer.last_timestamp(), base::Milliseconds(250));
  EXPECT_TRUE(observer.last_is_final());
  ASSERT_EQ(service_.artifacts().transcripts.size(), 1u);
  EXPECT_EQ(service_.artifacts().transcripts[0].text, "hello world");
  EXPECT_EQ(service_.artifacts().transcripts[0].timestamp,
            base::Milliseconds(250));

  service_.RemoveObserver(&observer);
  service_.OnTranscriptUpdateForTesting("ignored after remove",
                                        base::Milliseconds(400),
                                        /*is_final=*/false);
  EXPECT_EQ(observer.update_count(), 3);

  service_.StopRecording();

  service_.OnTranscriptUpdateForTesting("ignored after stop",
                                        base::Milliseconds(500),
                                        /*is_final=*/true);
  EXPECT_EQ(observer.update_count(), 3);
  EXPECT_EQ(service_.artifacts().transcripts.size(), 1u);

  // Starting a new recording session clears transcripts from the previous
  // session.
  service_.StartRecording();
  EXPECT_TRUE(service_.artifacts().transcripts.empty());
  service_.StopRecording();
}

TEST_F(SkillRecordingServiceTest, CapsRecordedTranscriptsAtMaxLimit) {
  service_.StartRecording();
  for (size_t i = 0; i < RecordingArtifacts::kMaxRecordedTranscripts; ++i) {
    service_.OnTranscriptUpdateForTesting("Segment", base::Milliseconds(100),
                                          /*is_final=*/true);
  }
  service_.OnTranscriptUpdateForTesting("Overflow segment",
                                        base::Milliseconds(200),
                                        /*is_final=*/true);

  service_.StopRecording();
  const RecordingArtifacts& result = service_.artifacts();
  ASSERT_EQ(result.transcripts.size(),
            RecordingArtifacts::kMaxRecordedTranscripts);
  EXPECT_EQ(result.transcripts.back().text, "Segment");
}

TEST_F(SkillRecordingServiceTest,
       GenerateSkillFromRecording_DelegatesRecordedSessionToModelExecutor) {
  service_.StartRecording();
  task_environment_.FastForwardBy(base::Milliseconds(150));
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");
  service_.StopRecording();

  ExpectExecuteModel(HasSubstr("Mouse Clicked at X:10, Y:20"),
                     "1. Click at (10, 20)");

  TestFuture<std::optional<std::string>> action_session_future;
  service_.GenerateSkillFromRecording(action_session_future.GetCallback());
  EXPECT_EQ(action_session_future.Get(), "1. Click at (10, 20)");

  // A session containing only finalized voice transcripts (no actions) must
  // also delegate to `ExecuteModel()` with only the new session's artifacts.
  service_.StartRecording();
  service_.OnTranscriptUpdateForTesting("Open settings",
                                        base::Milliseconds(100),
                                        /*is_final=*/true);
  service_.StopRecording();

  ExpectExecuteModel(AllOf(HasSubstr("Open settings"),
                           Not(HasSubstr("Mouse Clicked at X:10, Y:20"))),
                     "1. Open settings");

  TestFuture<std::optional<std::string>> transcript_session_future;
  service_.GenerateSkillFromRecording(transcript_session_future.GetCallback());
  EXPECT_EQ(transcript_session_future.Get(), "1. Open settings");
}

TEST_F(SkillRecordingServiceTest,
       StartRecordingCancelsInFlightSkillGeneration) {
  service_.StartRecording();
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "First session action");
  service_.StopRecording();

  // Capture the `ExecuteModel()` completion callback without invoking it to
  // keep the MES request suspended in flight after the `ThreadPool`
  // request-building task finishes.
  TestFuture<optimization_guide::OptimizationGuideModelExecutionResultCallback>
      execute_called_future;
  EXPECT_CALL(mock_model_executor_, ExecuteModel(_, _, _, _))
      .WillOnce(WithArg<3>(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            execute_called_future.SetValue(std::move(callback));
          }));

  bool cancelled_callback_ran = false;
  TestFuture<std::optional<std::string>> reentrant_future;
  // Verify recording state is already updated when `StartRecording()`
  // synchronously invokes the cancelled callback.
  service_.GenerateSkillFromRecording(base::BindLambdaForTesting(
      [&](std::optional<std::string> skill_instructions) {
        cancelled_callback_ran = true;
        EXPECT_EQ(skill_instructions, std::nullopt);
        EXPECT_TRUE(service_.is_recording());
        EXPECT_TRUE(service_.artifacts().actions.empty());

        // Re-entrant calls during cancellation must be safely rejected.
        service_.StartRecording();
        service_.GenerateSkillFromRecording(reentrant_future.GetCallback());
        EXPECT_FALSE(reentrant_future.IsReady());
      }));
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      stale_mes_callback = execute_called_future.Take();
  EXPECT_FALSE(cancelled_callback_ran);

  // Starting a new recording session must cancel the in-flight MES request from
  // the previous session and resolve its callback with `std::nullopt`.
  service_.StartRecording();
  EXPECT_TRUE(cancelled_callback_ran);
  EXPECT_EQ(reentrant_future.Get(), std::nullopt);
  service_.StopRecording();

  std::move(stale_mes_callback)
      .Run(CreateSkillGenerationMesResult("Stale response"), nullptr);
}

TEST_F(SkillRecordingServiceTest, ShutdownCancelsInFlightSkillGeneration) {
  service_.StartRecording();
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Recorded session action");
  service_.StopRecording();

  // Capture the `ExecuteModel()` completion callback without invoking it to
  // keep the MES request suspended in flight after the `ThreadPool`
  // request-building task finishes.
  TestFuture<optimization_guide::OptimizationGuideModelExecutionResultCallback>
      execute_called_future;
  EXPECT_CALL(mock_model_executor_, ExecuteModel(_, _, _, _))
      .WillOnce(WithArg<3>(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            execute_called_future.SetValue(std::move(callback));
          }));

  bool cancelled_callback_ran = false;
  service_.GenerateSkillFromRecording(base::BindLambdaForTesting(
      [&](std::optional<std::string> skill_instructions) {
        cancelled_callback_ran = true;
        EXPECT_EQ(skill_instructions, std::nullopt);
        // Re-entrant `Shutdown()` and `StartRecording()` during `Shutdown()`
        // must be safely ignored.
        service_.Shutdown();
        service_.StartRecording();
        EXPECT_FALSE(service_.is_recording());
      }));
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      stale_mes_callback = execute_called_future.Take();
  EXPECT_FALSE(cancelled_callback_ran);

  // Shutting down `SkillRecordingService` must cancel the in-flight MES request
  // and resolve its callback with `std::nullopt`.
  service_.Shutdown();
  EXPECT_TRUE(cancelled_callback_ran);

  std::move(stale_mes_callback)
      .Run(CreateSkillGenerationMesResult("Stale response"), nullptr);
}

TEST_F(SkillRecordingServiceTest,
       GenerateSkillFromRecording_EmptySessionOrAfterShutdown) {
  EXPECT_CALL(mock_model_executor_, ExecuteModel(_, _, _, _)).Times(0);

  TestFuture<std::optional<std::string>> empty_session_future;
  service_.GenerateSkillFromRecording(empty_session_future.GetCallback());
  EXPECT_FALSE(empty_session_future.IsReady());
  EXPECT_EQ(empty_session_future.Get(), std::nullopt);

  service_.StartRecording();
  service_.CaptureAction(SkillRecordingActionType::kLeftClick,
                         "Mouse Clicked at X:10, Y:20");

  // Calling `GenerateSkillFromRecording()` while a recording is still active
  // must fail gracefully without calling `ExecuteModel()`.
  TestFuture<std::optional<std::string>> active_recording_future;
  service_.GenerateSkillFromRecording(active_recording_future.GetCallback());
  EXPECT_FALSE(active_recording_future.IsReady());
  EXPECT_EQ(active_recording_future.Get(), std::nullopt);

  service_.StopRecording();

  // Calling `StartRecording()` or `GenerateSkillFromRecording()` after
  // `Shutdown()` must also fail gracefully without calling `ExecuteModel()`.
  service_.Shutdown();
  service_.StartRecording();
  EXPECT_FALSE(service_.is_recording());
  TestFuture<std::optional<std::string>> shutdown_future;
  service_.GenerateSkillFromRecording(shutdown_future.GetCallback());
  EXPECT_FALSE(shutdown_future.IsReady());
  EXPECT_EQ(shutdown_future.Get(), std::nullopt);

  // A service constructed without a `RemoteModelExecutor` must also return
  // `std::nullopt`.
  SkillRecordingService service_without_executor(&profile_,
                                                 /*model_executor=*/nullptr);
  service_without_executor.StartRecording();
  service_without_executor.CaptureAction(SkillRecordingActionType::kLeftClick,
                                         "Mouse Clicked at X:10, Y:20");
  service_without_executor.StopRecording();
  TestFuture<std::optional<std::string>> no_executor_future;
  service_without_executor.GenerateSkillFromRecording(
      no_executor_future.GetCallback());
  EXPECT_FALSE(no_executor_future.IsReady());
  EXPECT_EQ(no_executor_future.Get(), std::nullopt);
}
}  // namespace
}  // namespace context_hub
