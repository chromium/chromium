// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_task_test_utils.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@focusmode";
constexpr char kUser2Email[] = "user2@focusmode";

constexpr base::TimeDelta kShortDuration = base::Minutes(10);
constexpr base::TimeDelta kMediumDuration = base::Minutes(11);
constexpr base::TimeDelta kLongDuration = base::Minutes(30);

bool IsEndingMomentNudgeShown() {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      focus_mode_util::kFocusModeEndingMomentNudgeId);
}

AnchoredNudge* GetShownEndingMomentNudge() {
  CHECK(IsEndingMomentNudgeShown());
  return Shell::Get()->anchored_nudge_manager()->GetShownNudgeForTest(
      focus_mode_util::kFocusModeEndingMomentNudgeId);
}

// Simulate a media with the state of playing or paused based on `is_playing`.
// If `is_focus_mode_media` is true, the media is provided by the focus mode;
// otherwise, it will be a system media outside of the focus mode.
void SimulatePlaybackState(bool is_playing, bool is_focus_mode_media) {
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kActive;
  session_info->playback_state =
      is_playing ? media_session::mojom::MediaPlaybackState::kPlaying
                 : media_session::mojom::MediaPlaybackState::kPaused;

  if (is_focus_mode_media) {
    FocusModeController::Get()
        ->focus_mode_sounds_controller()
        ->MediaSessionInfoChanged(std::move(session_info));
  } else {
    FocusModeController::Get()->SetSystemMediaSessionInfoForTesting(
        std::move(session_info));
  }
}

// If `focus_gained` is true, the media with `id_observed` will gain the audio
// focus; otherwise, the request id for it will be released.
void SimulateAudioFocusGainedForSelectedPlaylist(
    bool focus_gained,
    const base::UnguessableToken& id_observed) {
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kActive;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;

  media_session::mojom::AudioFocusRequestStatePtr focus(
      media_session::mojom::AudioFocusRequestState::New());
  auto* controller = FocusModeController::Get();
  focus->request_id = id_observed;
  focus->session_info = std::move(session_info);

  auto* sounds_controller = controller->focus_mode_sounds_controller();
  if (focus_gained) {
    sounds_controller->OnFocusGained(std::move(focus));
  } else {
    sounds_controller->OnRequestIdReleased(id_observed);
  }
}

// Simulate start playing a newly selected playlist during an active session.
void SimulateStartPlaying() {
  auto* controller = FocusModeController::Get();
  controller->SetMediaSessionRequestIdForTesting(/*create_media_widget=*/true);
  SimulateAudioFocusGainedForSelectedPlaylist(
      /*focus_gained=*/true,
      /*id_observed=*/controller->GetMediaSessionRequestId());
}

// Simulate stop playing the selected playlist during an active session. Note
// that this function is to stop instead of pause the selected playlist. If you
// want to simulate pause the selected playlist, please call
// `SimulatePlaybackState`.
void SimulateStopPlaying() {
  auto* controller = FocusModeController::Get();
  SimulateAudioFocusGainedForSelectedPlaylist(
      /*focus_gained=*/false,
      /*id_observed=*/controller->GetMediaSessionRequestId());
  // Update the id for testing once we close the media widget.
  controller->SetMediaSessionRequestIdForTesting(/*create_media_widget=*/false);
}

}  // namespace

class FocusModeControllerMultiUserTest : public NoSessionAshTestBase {
 public:
  FocusModeControllerMultiUserTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_.InitWithFeatures(
        /*enabled_features=*/{features::kFocusMode, features::kFocusModeYTM},
        /*disabled_features=*/{});
  }
  ~FocusModeControllerMultiUserTest() override = default;

  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  // NoSessionAshTestBase:
  void SetUp() override {
    CHECK(test_web_view_factory_.get());
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // Focus Mode restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    user_1_prefs_ = nullptr;
    user_2_prefs_ = nullptr;
    NoSessionAshTestBase::TearDown();
  }

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    // Note that AdvanceClock() is used here instead of FastForwardBy() to
    // prevent long run time during an ash test session.
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  raw_ptr<TestingPrefServiceSimple> user_1_prefs_ = nullptr;
  raw_ptr<TestingPrefServiceSimple> user_2_prefs_ = nullptr;

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

// Tests that the default Focus Mode prefs are registered, and that they are
// read correctly by `FocusModeController`. Also test that switching users will
// load new user prefs.
TEST_F(FocusModeControllerMultiUserTest, LoadUserPrefsAndSwitchUsers) {
  auto& tasks_client1 = CreateFakeTasksClient(GetUser1AccountId());
  AddFakeTaskList(tasks_client1, "default");
  AddFakeTask(tasks_client1, "default", "task_id_1", "User1 Task");

  constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);
  constexpr bool kDefaultDNDState = true;
  const focus_mode_util::SoundType kUser1SoundType =
      focus_mode_util::SoundType::kSoundscape;

  constexpr base::TimeDelta kUser2SessionDuration = base::Minutes(200);
  constexpr bool kUser2DNDState = false;
  const focus_mode_util::SoundType kUser2SoundType =
      focus_mode_util::SoundType::kYouTubeMusic;

  base::Value::Dict user2_task_dict;
  const std::string task_list_id_2 = "task_list_id_2";
  const std::string task_id_2 = "task_id_2";
  user2_task_dict.Set(focus_mode_util::kTaskListIdKey, task_list_id_2);
  user2_task_dict.Set(focus_mode_util::kTaskIdKey, task_id_2);

  // Set the secondary user2's Focus Mode prefs.
  user_2_prefs()->SetTimeDelta(prefs::kFocusModeSessionDuration,
                               kUser2SessionDuration);
  user_2_prefs()->SetBoolean(prefs::kFocusModeDoNotDisturb, kUser2DNDState);
  user_2_prefs()->SetDict(prefs::kFocusModeSelectedTask,
                          user2_task_dict.Clone());
  base::Value::Dict sound_section_dict;
  sound_section_dict.Set(focus_mode_util::kSoundTypeKey,
                         static_cast<int>(kUser2SoundType));
  user_2_prefs()->SetDict(prefs::kFocusModeSoundSection,
                          std::move(sound_section_dict));

  // Verify the sound section dictionary values for user2.
  sound_section_dict =
      user_2_prefs()->GetDict(prefs::kFocusModeSoundSection).Clone();
  EXPECT_FALSE(sound_section_dict.empty());
  EXPECT_EQ(static_cast<int>(kUser2SoundType),
            sound_section_dict.FindInt(focus_mode_util::kSoundTypeKey).value());

  // Log in and check to see that the user1 prefs are the default values, since
  // there should have been nothing previously.
  SimulateUserLogin(GetUser1AccountId());
  EXPECT_EQ(kDefaultSessionDuration,
            user_1_prefs()->GetTimeDelta(prefs::kFocusModeSessionDuration));
  EXPECT_EQ(kDefaultDNDState,
            user_1_prefs()->GetBoolean(prefs::kFocusModeDoNotDisturb));
  EXPECT_TRUE(user_1_prefs()->GetDict(prefs::kFocusModeSelectedTask).empty());
  EXPECT_TRUE(user_1_prefs()->GetDict(prefs::kFocusModeSoundSection).empty());

  // Verify that `FocusModeController` has loaded the user prefs.
  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();
  EXPECT_EQ(kDefaultSessionDuration, controller->GetSessionDuration());
  EXPECT_EQ(kDefaultDNDState, controller->turn_on_do_not_disturb());
  EXPECT_FALSE(controller->HasSelectedTask());
  EXPECT_EQ(kUser1SoundType, sounds_controller->sound_type());

  // Switch users and verify that `FocusModeController` has loaded the new user
  // prefs.
  auto& tasks_client2 = CreateFakeTasksClient(GetUser2AccountId());
  AddFakeTaskList(tasks_client2, "task_list_id_2");
  AddFakeTask(tasks_client2, "task_list_id_2", "task_id_2", "User2 Task");
  SwitchActiveUser(GetUser2AccountId());
  // Wait for the `UpdateFromUserPrefs()` PostTask to finish.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(kUser2SessionDuration, controller->GetSessionDuration());
  EXPECT_EQ(kUser2DNDState, controller->turn_on_do_not_disturb());
  EXPECT_EQ(task_list_id_2,
            controller->tasks_model().selected_task()->task_id.list_id);
  EXPECT_EQ(task_id_2, controller->tasks_model().selected_task()->task_id.id);
  EXPECT_EQ(kUser2SoundType, sounds_controller->sound_type());
}

// Tests that switching users will first clear all cached tasks data, then load
// new user prefs and update the tasks caches.
TEST_F(FocusModeControllerMultiUserTest, SwitchingUsersClearsTasksCache) {
  // Setup for user1.
  auto& tasks_client1 = CreateFakeTasksClient(GetUser1AccountId());
  base::Value::Dict user1_task_dict;
  const std::string task_list_id_1 = "task_list_id_1";
  const std::string task_id_1 = "task_id_1";
  user1_task_dict.Set(focus_mode_util::kTaskListIdKey, task_list_id_1);
  user1_task_dict.Set(focus_mode_util::kTaskIdKey, task_id_1);
  // Set the primary user1's Focus Mode selected task pref.
  user_1_prefs()->SetDict(prefs::kFocusModeSelectedTask,
                          user1_task_dict.Clone());
  AddFakeTaskList(tasks_client1, task_list_id_1);
  AddFakeTask(tasks_client1, task_list_id_1, task_id_1, "User1 Task");

  // Login as the primary user1.
  SimulateUserLogin(GetUser1AccountId());

  // Wait for `UpdateFromUserPrefs()` to finish before requesting to update the
  // provider cache.
  task_environment()->RunUntilIdle();
  auto* controller = FocusModeController::Get();
  EXPECT_TRUE(controller->HasSelectedTask());
  EXPECT_EQ(task_list_id_1,
            controller->tasks_model().selected_task()->task_id.list_id);
  EXPECT_EQ(task_id_1, controller->tasks_model().selected_task()->task_id.id);
  EXPECT_FALSE(controller->tasks_model().tasks().empty());
  controller->RequestTasksUpdateForTesting();
  EXPECT_TRUE(controller->TasksProviderHasCachedTasksForTesting());

  // Setup for user2.
  auto& tasks_client2 = CreateFakeTasksClient(GetUser2AccountId());
  base::Value::Dict user2_task_dict;
  const std::string task_list_id_2 = "task_list_id_2";
  const std::string task_id_2 = "task_id_2";
  user2_task_dict.Set(focus_mode_util::kTaskListIdKey, task_list_id_2);
  user2_task_dict.Set(focus_mode_util::kTaskIdKey, task_id_2);
  // Set the secondary user2's Focus Mode selected task pref.
  user_2_prefs()->SetDict(prefs::kFocusModeSelectedTask,
                          user2_task_dict.Clone());
  AddFakeTaskList(tasks_client2, task_list_id_2);
  AddFakeTask(tasks_client2, task_list_id_2, task_id_2, "User2 Task");

  // Switch users and verify that all the tasks caches are cleared.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(controller->tasks_model().tasks().empty());
  EXPECT_FALSE(controller->tasks_model().selected_task());
  EXPECT_FALSE(controller->TasksProviderHasCachedTasksForTesting());

  // Wait for `UpdateFromUserPrefs()` to finish before requesting to update the
  // provider cache.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(task_list_id_2,
            controller->tasks_model().selected_task()->task_id.list_id);
  EXPECT_EQ(task_id_2, controller->tasks_model().selected_task()->task_id.id);
  EXPECT_FALSE(controller->tasks_model().tasks().empty());
  controller->RequestTasksUpdateForTesting();
  EXPECT_TRUE(controller->TasksProviderHasCachedTasksForTesting());
}

// Tests that when the user selects a different type of playlist, the user pref
// for the sound section will be updated for this change.
TEST_F(FocusModeControllerMultiUserTest, TogglePlaylistToChangeUserPref) {
  SimulateUserLogin(GetUser1AccountId());
  const focus_mode_util::SoundType kUser1SoundType =
      focus_mode_util::SoundType::kSoundscape;
  const focus_mode_util::SoundType kUser1NewSoundType =
      focus_mode_util::SoundType::kYouTubeMusic;

  // Verify that `FocusModeSoundsController` has loaded the user prefs.
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  EXPECT_EQ(kUser1SoundType, sounds_controller->sound_type());

  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  selected_playlist.state = focus_mode_util::SoundState::kNone;

  // 1. Toggle a YouTube Music type of playlists and verify the pref.
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_EQ(kUser1NewSoundType, sounds_controller->sound_type());

  // The playlist id should be also updated into the user pref.
  base::Value::Dict dict =
      user_1_prefs()->GetDict(prefs::kFocusModeSoundSection).Clone();
  EXPECT_EQ(static_cast<int>(kUser1NewSoundType),
            dict.FindInt(focus_mode_util::kSoundTypeKey).value());
  EXPECT_EQ(selected_playlist.id,
            *(dict.FindString(focus_mode_util::kPlaylistIdKey)));

  // 2. Toggle the selected playlist to deselect it and verify the pref.
  selected_playlist.state = focus_mode_util::SoundState::kSelected;
  sounds_controller->TogglePlaylist(selected_playlist);
  dict = user_1_prefs()->GetDict(prefs::kFocusModeSoundSection).Clone();
  EXPECT_EQ(static_cast<int>(kUser1NewSoundType),
            dict.FindInt(focus_mode_util::kSoundTypeKey).value());
  EXPECT_EQ(std::string(), *(dict.FindString(focus_mode_util::kPlaylistIdKey)));
}

TEST_F(FocusModeControllerMultiUserTest, ToggleClosesSystemBubble) {
  SimulateUserLogin(GetUser1AccountId());

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Show the bubble.
  auto* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();

  // Toggle focus mode on, and verify that the bubble is closed.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(system_tray->IsBubbleShown());

  // Show the bubble again.
  system_tray->ShowBubble();

  // Toggle focus mode off, and verify that this doesn't affect the bubble
  // visibility.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(system_tray->IsBubbleShown());
}

// Tests that we can determine if a focus session has started before.
TEST_F(FocusModeControllerMultiUserTest, FirstTimeUserFlow) {
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->HasStartedSessionBefore());

  FocusModeController::Get()->ToggleFocusMode();
  EXPECT_TRUE(controller->HasStartedSessionBefore());
}

// Tests adding and completing tasks, and the changes for the user pref.
TEST_F(FocusModeControllerMultiUserTest, TasksFlow) {
  SimulateUserLogin(GetUser1AccountId());

  const std::string task_list_id = "list1";
  const std::string task_id = "task1";
  const std::string title = "Focus Task";

  auto& tasks_client = CreateFakeTasksClient(GetUser1AccountId());
  tasks_client.set_http_error(google_apis::ApiErrorCode::HTTP_SUCCESS);
  AddFakeTaskList(tasks_client, task_list_id);
  AddFakeTask(tasks_client, task_list_id, task_id, title);

  FocusModeTask task;
  task.task_id = {.list_id = task_list_id, .id = task_id};
  task.title = title;
  task.updated = base::Time::Now();

  // Verify that initially there is no selected task.
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->HasSelectedTask());

  // Select a task, and verify that the task data is accurate.
  controller->tasks_model().SetSelectedTask(task);

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Verify the selected task info is accurate in the user pref once we start a
  // focus session.
  base::Value::Dict task_dict =
      user_1_prefs()->GetDict(prefs::kFocusModeSelectedTask).Clone();
  EXPECT_FALSE(task_dict.empty());
  EXPECT_EQ(task_list_id,
            *(task_dict.FindString(focus_mode_util::kTaskListIdKey)));
  EXPECT_EQ(task_id, *(task_dict.FindString(focus_mode_util::kTaskIdKey)));

  // Complete the task during the session, and verify that the task data is
  // cleared.
  controller->CompleteTask();
  EXPECT_FALSE(controller->HasSelectedTask());
  task_dict = user_1_prefs()->GetDict(prefs::kFocusModeSelectedTask).Clone();
  EXPECT_TRUE(task_dict.empty());
}

// Tests basic ending moment functionality. Includes verifying that the ending
// moment is persistent.
TEST_F(FocusModeControllerMultiUserTest, EndingMoment) {
  SimulateUserLogin(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);
  EXPECT_FALSE(controller->current_session().has_value());

  // Toggling focus mode on creates a `current_session`. Verify that we are not
  // in the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->current_session().has_value());
  EXPECT_TRUE(controller->in_focus_session());

  // Once the session expires, the ending moment should start.
  AdvanceClock(kSessionDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // Verifies that the ending moment terminates after the specified duration and
  // that there is no longer an existing `current_session`.

  // Verifies that DND terminates after the initial specified duration, but the
  // ending moment is persistent.
  AdvanceClock(focus_mode_util::kInitialEndingMomentDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  AdvanceClock(base::Minutes(100));
  EXPECT_TRUE(controller->in_ending_moment());
}

// Tests that we can start a new/separate focus session during an ongoing ending
// moment.
TEST_F(FocusModeControllerMultiUserTest, StartNewSessionDuringEndingMoment) {
  SimulateUserLogin(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  // Case 1: Normal ending moment timeout.
  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);
  EXPECT_FALSE(controller->current_session().has_value());

  // Toggle focus mode on, and verify that we are not in the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Once the session expires, the ending moment should start.
  AdvanceClock(kSessionDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // Toggling focus mode during the ending moment will start a new session and
  // terminate the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->current_session().has_value());
  EXPECT_TRUE(controller->in_focus_session());
}

// Tests basic ending moment nudge functionality. Includes the nudge appearing
// and disappearing.
TEST_F(FocusModeControllerMultiUserTest, EndingMomentNudgeTest) {
  SimulateUserLogin(GetUser1AccountId());
  CreateFakeTasksClient(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);

  auto trigger_ending_moment = [this, controller, kSessionDuration]() {
    // Toggling focus mode on and verify that we are not in the ending moment.
    controller->ToggleFocusMode();
    EXPECT_TRUE(controller->in_focus_session());

    // Once the session expires, the ending moment should start.
    AdvanceClock(kSessionDuration);
    EXPECT_TRUE(controller->in_ending_moment());
  };

  // Verify that the nudge is visible when the ending moment is triggered.
  trigger_ending_moment();
  EXPECT_TRUE(IsEndingMomentNudgeShown());

  // After the ending moment nudge shown, we ignore the nudge view for
  // accessibility purposes.
  EXPECT_EQ(ax::mojom::Role::kNone,
            GetShownEndingMomentNudge()->GetAccessibleWindowRole());

  // Verify that the nudge hides.
  AdvanceClock(focus_mode_util::kInitialEndingMomentDuration);
  EXPECT_FALSE(IsEndingMomentNudgeShown());

  controller->ResetFocusSession();

  // Trigger the ending moment, then check that the notification disappears when
  // we open the tray bubble.
  trigger_ending_moment();
  EXPECT_TRUE(IsEndingMomentNudgeShown());
  FocusModeTray* focus_mode_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  LeftClickOn(focus_mode_tray);
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_FALSE(IsEndingMomentNudgeShown());
}

// Verify that when toggling a focus mode, the histogram will record the initial
// session duration.
TEST_F(FocusModeControllerMultiUserTest, CheckInitialDurationHistograms) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // 1. Start a focus session with the minimum session duration.
  controller->SetInactiveSessionDuration(focus_mode_util::kMinimumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      focus_mode_util::kMinimumDuration.InMinutes(), 1);

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a new focus session with the maximum session duration.
  controller->SetInactiveSessionDuration(focus_mode_util::kMaximumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      focus_mode_util::kMaximumDuration.InMinutes(), 1);

  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      2);
}

// Tests that the histogram will record how many tasks we selected during a
// focus session.
TEST_F(FocusModeControllerMultiUserTest, CheckTasksSelectedHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Start a focus session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(controller->HasSelectedTask());

  // Select a task during the session.
  FocusModeTask task;
  task.task_id = {.list_id = "abc", .id = "1"};
  task.title = "Focus Task";
  task.updated = base::Time::Now();

  controller->SetSelectedTask(task);
  EXPECT_TRUE(controller->HasSelectedTask());

  // Remove the task and select a new one.
  controller->SetSelectedTask({});

  task.task_id = {.list_id = "abc", .id = "2"};
  task.title = "A New Focus Task";
  task.updated = base::Time::Now();
  controller->SetSelectedTask(task);

  // End the focus session and check the count.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kTasksSelectedHistogramName, /*sample=*/2,
      /*expected_count=*/1);
}

// Verify that when a session is over, the histogram will record which type of
// the DND state.
TEST_F(FocusModeControllerMultiUserTest, CheckDNDStateOnFocusEndHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* message_center = message_center::MessageCenter::Get();

  // 1. FocusModeOn - DND enabled by FocusMode (default behavior).
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->turn_on_do_not_disturb());
  EXPECT_FALSE(message_center->IsQuietMode());

  // Turning on Focus Mode will also turn on DND by default.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kFocusModeOn,
      /*expected_count=*/1);

  // 2. AlreadyOn - DND was already on before FocusMode started and was on when
  // we finished (with NO user interaction during the session).
  message_center->SetQuietMode(true);
  EXPECT_TRUE(message_center->IsQuietMode());

  // Start and end a Focus Mode session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOn,
      /*expected_count=*/1);

  // 3. AlreadyOff - DND was off when FocusMode started, and is still off (with
  // NO user interactions during the session).
  message_center->SetQuietMode(false);
  EXPECT_FALSE(message_center->IsQuietMode());

  // Set `turn_on_do_not_disturb` to prevent Focus Mode from automatically
  // turning on DND when a session starts.
  controller->set_turn_on_do_not_disturb(false);

  // Start a session and end it.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOff,
      /*expected_count=*/1);

  // 4. TurnedOn - The user manually toggled DND during the focus session, and
  // the session ends with DND on.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());

  // Turn on DND during the session.
  message_center->SetQuietMode(true);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOn,
      /*expected_count=*/1);

  // 5. TurnedOff - The user manually toggled DND during the focus session, and
  // the session ends with DND off.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Turn off DND during the session.
  message_center->SetQuietMode(false);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOff,
      /*expected_count=*/1);
}

// Verify that when a session is over, the histogram will record how many
// minutes the user extended during the session.
TEST_F(FocusModeControllerMultiUserTest, CheckTimeAddedOnSessionEndHistogram) {
  base::HistogramTester histogram_tester;

  // 1. Start a session with 10 minutes and extend the session duration while in
  // an active session.
  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kShortDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kShortDuration, controller->session_duration());

  controller->ExtendSessionDuration();
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(/*name=*/"Ash.FocusMode.TimeAdded.Short",
                                     /*sample=*/10, /*expected_count=*/1);

  // 2. Start a session with 11 minutes. Even not extending the session will
  // still trigger the metric to be recorded.
  controller->SetInactiveSessionDuration(kMediumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kMediumDuration, controller->session_duration());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(/*name=*/"Ash.FocusMode.TimeAdded.Medium",
                                     /*sample=*/0, /*expected_count=*/1);

  // 3. Start a session with 30 minutes and extend the session.
  controller->SetInactiveSessionDuration(kLongDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kLongDuration, controller->session_duration());

  controller->ExtendSessionDuration();
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(/*name=*/"Ash.FocusMode.TimeAdded.Long",
                                     /*sample=*/10, /*expected_count=*/1);
}

// Verify that when a session is over, the histogram will record the session
// progress percentage.
TEST_F(FocusModeControllerMultiUserTest, CheckPercentCompletedHistogram) {
  base::HistogramTester histogram_tester;

  // 1. Start a session with 10 minutes and stop the session when the progress
  // is 50%.
  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kShortDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kShortDuration, controller->session_duration());

  AdvanceClock(base::Minutes(5));
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/"Ash.FocusMode.PercentOfSessionCompleted.Short",
      /*sample=*/50, /*expected_count=*/1);

  // 2. Start a session with 11 minutes and stop the session immediately, which
  // should record 0%.
  controller->SetInactiveSessionDuration(kMediumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kMediumDuration, controller->session_duration());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/"Ash.FocusMode.PercentOfSessionCompleted.Medium",
      /*sample=*/0, /*expected_count=*/1);

  // 3. Start a session with 30 minutes and extend the session two times. After
  // the ending moment, the histogram should record 100%.
  controller->SetInactiveSessionDuration(kLongDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(kLongDuration, controller->session_duration());

  controller->ExtendSessionDuration();
  controller->ExtendSessionDuration();

  const auto& current_session = controller->current_session();
  EXPECT_TRUE(current_session.has_value());

  // Once the session expires, the ending moment should start.
  AdvanceClock(current_session->session_duration());
  EXPECT_TRUE(controller->in_ending_moment());

  // After the ending moment terminates, the histogram will record 100%.
  controller->ResetFocusSession();
  histogram_tester.ExpectBucketCount(
      /*name=*/"Ash.FocusMode.PercentOfSessionCompleted.Long",
      /*sample=*/100, /*expected_count=*/1);
}

// Verify that the histogram will record the number of tasks completed during an
// active session and the ending moment.
TEST_F(FocusModeControllerMultiUserTest, CheckTasksCompletedHistogram) {
  base::HistogramTester histogram_tester;

  SimulateUserLogin(GetUser1AccountId());

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // 1. Select a new task before a session starts, which will not be recorded
  // into the histogram.
  auto& tasks_client = CreateFakeTasksClient(GetUser1AccountId());
  tasks_client.set_http_error(google_apis::ApiErrorCode::HTTP_SUCCESS);

  FocusModeTask task;
  task.task_id = {.list_id = "list0", .id = "task0"};
  task.title = "Focus Task";
  task.updated = base::Time::Now();

  AddFakeTaskList(tasks_client, task.task_id.list_id);
  AddFakeTask(tasks_client, task.task_id.list_id, task.task_id.id, task.title);
  controller->RequestTasksUpdateForTesting();

  controller->SetSelectedTask(task);
  controller->CompleteTask();
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kTasksCompletedHistogramName, 0);

  // 2. Start a focus session and select a task during the active session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  task.task_id = {.list_id = "list1", .id = "task1"};
  task.title = "A New Focus Task";
  task.updated = base::Time::Now();

  AddFakeTaskList(tasks_client, task.task_id.list_id);
  AddFakeTask(tasks_client, task.task_id.list_id, task.task_id.id, task.title);

  controller->SetSelectedTask(task);
  EXPECT_TRUE(controller->HasSelectedTask());

  // Mark the first task as completed.
  controller->CompleteTask();

  // Select a new task during this session.
  task.task_id = {.list_id = "list2", .id = "task2"};
  task.title = "A New Focus Task";
  task.updated = base::Time::Now();

  AddFakeTaskList(tasks_client, task.task_id.list_id);
  AddFakeTask(tasks_client, task.task_id.list_id, task.task_id.id, task.title);

  controller->SetSelectedTask(task);

  // 3. Mark the second task as completed during the ending moment.
  const auto& current_session = controller->current_session();
  EXPECT_TRUE(current_session.has_value());
  AdvanceClock(current_session->session_duration());
  EXPECT_TRUE(controller->in_ending_moment());
  controller->CompleteTask();

  // Verify the histogram after the ending moment terminates.
  controller->ResetFocusSession();
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kTasksCompletedHistogramName,
      /*sample=*/2, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kTasksCompletedHistogramName, 1);
}

TEST_F(FocusModeControllerMultiUserTest, CheckSessionDurationHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kShortDuration);

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  const auto& current_session = controller->current_session();
  EXPECT_TRUE(current_session.has_value());

  AdvanceClock(base::Minutes(2));
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kSessionDurationHistogramName, /*sample=*/2,
      /*expected_count=*/1);
}

TEST_F(FocusModeControllerMultiUserTest,
       CheckEndingMomentBubbleActionHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kShortDuration);

  // 1. Bubble was opened and closed.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  AdvanceClock(kShortDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // After the ending moment terminates, the histogram will record 100%.
  controller->ResetFocusSession();
  EXPECT_FALSE(controller->in_ending_moment());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kEndingMomentBubbleActionHistogram,
      /*sample=*/
      focus_mode_histogram_names::EndingMomentBubbleClosedReason::kOpended,
      /*expected_count=*/1);

  // 2. Bubble was opened and minutes were added to the session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  AdvanceClock(kShortDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // Extend the session during the ending moment.
  controller->ExtendSessionDuration();
  EXPECT_FALSE(controller->in_ending_moment());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kEndingMomentBubbleActionHistogram,
      /*sample=*/
      focus_mode_histogram_names::EndingMomentBubbleClosedReason::kExtended,
      /*expected_count=*/1);
}

// Verify the histogram will record the different state when a session starts.
TEST_F(FocusModeControllerMultiUserTest, CheckStartedWithTaskHistogram) {
  base::HistogramTester histogram_tester;

  SimulateUserLogin(GetUser1AccountId());

  auto* controller = FocusModeController::Get();

  // 1. Start a session without a task selected.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(controller->HasSelectedTask());

  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kStartedWithTaskStatekHistogramName,
      /*sample=*/focus_mode_histogram_names::StartedWithTaskState::kNoTask,
      /*expected_count=*/1);

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a session with a newly selected task.
  auto& tasks_client = CreateFakeTasksClient(GetUser1AccountId());

  FocusModeTask task;
  task.task_id = {.list_id = "list0", .id = "task0"};
  task.title = "Focus Task";
  task.updated = base::Time::Now();

  AddFakeTaskList(tasks_client, task.task_id.list_id);
  AddFakeTask(tasks_client, task.task_id.list_id, task.task_id.id, task.title);
  controller->SetSelectedTask(task);

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(controller->HasSelectedTask());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kStartedWithTaskStatekHistogramName,
      /*sample=*/
      focus_mode_histogram_names::StartedWithTaskState::kNewlySelectedTask,
      /*expected_count=*/1);

  // Mark this selected task as completed, and select a new task during this
  // session. Don't finish this selected task until the next session.
  task.task_id = {.list_id = "list1", .id = "task1"};
  task.title = "A New Focus Task";
  task.updated = base::Time::Now();

  AddFakeTaskList(tasks_client, task.task_id.list_id);
  AddFakeTask(tasks_client, task.task_id.list_id, task.task_id.id, task.title);

  controller->SetSelectedTask(task);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->HasSelectedTask());

  // 3. Start a session with the previously selected task.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kStartedWithTaskStatekHistogramName,
      /*sample=*/
      focus_mode_histogram_names::StartedWithTaskState::kPreviouslySelectedTask,
      /*expected_count=*/1);
}

// Tests that when starting a focus session, it records if there is an existing
// media playing.
TEST_F(FocusModeControllerMultiUserTest, CheckMediaPlayingOnStartsHistogram) {
  base::HistogramTester histogram_tester;

  // 1. Start a focus session without a system media.
  auto* controller = FocusModeController::Get();
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::
          kStartedWithExistingMediaPlayingHistogramName,
      /*sample=*/false, /*expected_count=*/1);

  // End the focus session.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a focus session with a system media playing.
  SimulatePlaybackState(/*is_playing=*/true, /*is_focus_mode_media=*/false);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::
          kStartedWithExistingMediaPlayingHistogramName,
      /*sample=*/true, /*expected_count=*/1);

  // End the focus session.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 3. Start a focus session with a system media paused.
  SimulatePlaybackState(/*is_playing=*/false, /*is_focus_mode_media=*/false);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::
          kStartedWithExistingMediaPlayingHistogramName,
      /*sample=*/false, /*expected_count=*/2);
}

// Tests that the latency for playing a playlist will be recorded on session
// starts and during an active session.
TEST_F(FocusModeControllerMultiUserTest, CheckPlaylistPlayedLatencyHistograms) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();

  // 1. Simulate that we have a selected playlist with the soundscape type
  // before starting a focus session, the histogram will record the latency.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  selected_playlist.state = focus_mode_util::SoundState::kNone;
  sounds_controller->TogglePlaylist(selected_playlist);

  controller->ToggleFocusMode();
  SimulateStartPlaying();
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kSoundscapeLatencyInMillisecondsHistogramName,
      1);

  // 2. Simulate that we select another soundscape playlist to play during the
  // active session, the histogram will record the latency.
  SimulateStopPlaying();

  selected_playlist.id = "id1";
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulateStartPlaying();
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kSoundscapeLatencyInMillisecondsHistogramName,
      2);

  // Simulate we stop the selected playlist with the soundscape type.
  SimulateStopPlaying();

  // 3. Simulate that we select a YouTube Music type of playlist to play during
  // the active session.
  selected_playlist.id = "id2";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulateStartPlaying();
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::
          kYouTubeMusicLatencyInMillisecondsHistogramName,
      1);

  // Simulate we stop the current selected playlist and end the current focus
  // session.
  SimulateStopPlaying();

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 4. Simulate that we select a YouTube Music and then start a new focus
  // session.
  selected_playlist.id = "id3";
  sounds_controller->TogglePlaylist(selected_playlist);

  controller->ToggleFocusMode();
  SimulateStartPlaying();
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::
          kYouTubeMusicLatencyInMillisecondsHistogramName,
      2);
}

TEST_F(FocusModeControllerMultiUserTest,
       CheckPlaylistPausedEventDuringSessionHitogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();

  // 1. Start a focus session and pause the playlist during the session.
  int count_pause_event = 0;
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Select a playlist and pause it for once.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  selected_playlist.state = focus_mode_util::SoundState::kNone;
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulatePlaybackState(/*is_playing=*/true, /*is_focus_mode_media=*/true);

  SimulatePlaybackState(/*is_playing=*/false, /*is_focus_mode_media=*/true);
  count_pause_event++;

  // Select another playlist and pause it for twice.
  selected_playlist.id = "id1";
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulatePlaybackState(/*is_playing=*/true, /*is_focus_mode_media=*/true);

  SimulatePlaybackState(/*is_playing=*/false, /*is_focus_mode_media=*/true);
  count_pause_event++;
  SimulatePlaybackState(/*is_playing=*/true, /*is_focus_mode_media=*/true);
  SimulatePlaybackState(/*is_playing=*/false, /*is_focus_mode_media=*/true);
  count_pause_event++;

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kMusicPausedEventsCount,
      /*sample=*/count_pause_event, /*expected_count=*/1);

  // 2. Start a new focus session, and we don't pause the playlist during the
  // session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  SimulatePlaybackState(/*is_playing=*/true, /*is_focus_mode_media=*/true);

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kMusicPausedEventsCount,
      /*sample=*/0, /*expected_count=*/1);
}

TEST_F(FocusModeControllerMultiUserTest, CheckPlaylistChosenHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Simulate we play a soundscape 2 in session.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id2";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  selected_playlist.state = focus_mode_util::SoundState::kNone;
  selected_playlist.list_position = 1;
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulateStartPlaying();
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistChosenHistogram, /*sample=*/
      focus_mode_histogram_names::FocusModePlaylistChosen::kSoundscapes2,
      /*expected_count=*/1);
  SimulateStopPlaying();

  // Simulate we play a YouTube Music 3 in session.
  selected_playlist.id = "id3";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  selected_playlist.list_position = 2;
  sounds_controller->TogglePlaylist(selected_playlist);
  SimulateStartPlaying();
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistChosenHistogram, /*sample=*/
      focus_mode_histogram_names::FocusModePlaylistChosen::kYouTubeMusic3,
      /*expected_count=*/1);
}

}  // namespace ash
