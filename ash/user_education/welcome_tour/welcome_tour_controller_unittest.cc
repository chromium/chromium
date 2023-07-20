// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_notification.h"
#include "ash/test/test_widget_builder.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/mock_welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Aliases.
using ::base::test::RunOnceClosure;
using ::session_manager::SessionState;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Matches;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::user_education::HelpBubbleArrow;
using ::user_education::TutorialDescription;
using ::views::test::WidgetDestroyedWaiter;

using ContextMode = TutorialDescription::ContextMode;
using ElementSpecifier = TutorialDescription::ElementSpecifier;

// Actions ---------------------------------------------------------------------

// TODO(http://b/277094923): Try to promote to //base/test/gmock_move_support.h.
// Existing support is limited in that the gMock framework provides const-only
// access to `args` for all except the last action. This action lessens the
// effect of that limitation by supporting multiple moves at a time.
template <size_t... I, typename... T>
auto MoveArgs(T*... out) {
  return [out...](auto&&... args) {
    // Assigns the Ith-indexed value from `args` to `out` parameters by move.
    ([&]() { *out = std::move(std::get<I>(std::tie(args...))); }(), ...);
  };
}

// Matchers --------------------------------------------------------------------

MATCHER_P(ElementSpecifierEq, element_specifier, "") {
  return absl::visit(base::Overloaded{
                         [&](const ui::ElementIdentifier& element_id) {
                           return arg.element_id() == element_id &&
                                  arg.element_name().empty();
                         },
                         [&](const std::string& element_name) {
                           return arg.element_name() == element_name &&
                                  arg.element_id() == ui::ElementIdentifier();
                         },
                     },
                     element_specifier);
}

MATCHER_P6(BubbleStep,
           element_specifier,
           context_mode,
           help_bubble_id,
           body_text_id,
           arrow,
           has_next_button,
           "") {
  namespace util = user_education_util;
  return arg.step_type() == ui::InteractionSequence::StepType::kShown &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode &&
         util::GetHelpBubbleId(arg.extended_properties()) == help_bubble_id &&
         arg.body_text_id() == body_text_id && arg.arrow() == arrow &&
         arg.next_button_callback().is_null() != has_next_button;
}

MATCHER_P2(HiddenStep, element_specifier, context_mode, "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kHidden &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode;
}

MATCHER_P3(EventStep,
           element_specifier,
           context_mode,
           has_name_elements_callback,
           "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kCustomEvent &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode &&
         arg.name_elements_callback().is_null() != has_name_elements_callback;
}

MATCHER_P2(ShownStep, element_specifier, context_mode, "") {
  return arg.step_type() == ui::InteractionSequence::StepType::kShown &&
         Matches(ElementSpecifierEq(element_specifier))(arg) &&
         arg.context_mode() == context_mode;
}

// MockView --------------------------------------------------------------------

// A mocked view to expose received events.
class MockView : public views::View {
 public:
  MOCK_METHOD(void, OnEvent, (ui::Event*), (override));
};

// Helpers ---------------------------------------------------------------------

// Adds a simple notification to the `message_center::MessageCenter` with the
// given `id`. If the optional argument `is_system_priority` is true, it will be
// set to the highest priority.
void AddNotification(const std::string& id, bool is_system_priority = false) {
  auto notification =
      SystemNotificationBuilder()
          .SetId(id)
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetDelegate(
              base::MakeRefCounted<message_center::NotificationDelegate>())
          .BuildPtr(false);
  if (is_system_priority) {
    notification->SetSystemPriority();
  }
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// Checks the given expected notification counts against the current actual
// values in `message_center::MessageCenter`.
void ExpectNotificationCounts(size_t total_notifications,
                              size_t visible_notifications,
                              size_t popup_notifications) {
  auto* message_center = message_center::MessageCenter::Get();
  EXPECT_EQ(message_center->GetNotifications().size(), total_notifications);
  EXPECT_EQ(message_center->GetVisibleNotifications().size(),
            visible_notifications);
  EXPECT_EQ(message_center->GetPopupNotifications().size(),
            popup_notifications);
}

}  // namespace

// WelcomeTourControllerTest ---------------------------------------------------

// Base class for tests of the `WelcomeTourController`.
class WelcomeTourControllerTest : public UserEducationAshTestBase {
 public:
  WelcomeTourControllerTest() {
    // NOTE: The `WelcomeTourController` exists only when the Welcome Tour
    // feature is enabled. Controller existence is verified in test coverage
    // for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kWelcomeTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(WelcomeTourControllerTest, GetTutorialDescriptions) {
  auto* welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  std::map<TutorialId, TutorialDescription> tutorial_descriptions_by_id =
      static_cast<UserEducationFeatureController*>(welcome_tour_controller)
          ->GetTutorialDescriptions();

  // TODO(http://b/275616974): Implement tutorial descriptions.
  EXPECT_EQ(tutorial_descriptions_by_id.size(), 1u);
  EXPECT_THAT(
      tutorial_descriptions_by_id,
      Contains(Pair(
          Eq(TutorialId::kWelcomeTourPrototype1),
          Field(
              &TutorialDescription::steps,
              ElementsAre(
                  ShownStep(ElementSpecifier(kWelcomeTourDialogElementId),
                            ContextMode::kAny),
                  HiddenStep(ElementSpecifier(kWelcomeTourDialogElementId),
                             ContextMode::kFromPreviousStep),
                  BubbleStep(ElementSpecifier(kShelfViewElementId),
                             ContextMode::kInitial,
                             HelpBubbleId::kWelcomeTourShelf,
                             IDS_ASH_WELCOME_TOUR_SHELF_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kShelfViewElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/true),
                  BubbleStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourStatusArea,
                             IDS_ASH_WELCOME_TOUR_STATUS_AREA_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kUnifiedSystemTrayElementName),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/true),
                  BubbleStep(ElementSpecifier(kHomeButtonElementName),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourHomeButton,
                             IDS_ASH_WELCOME_TOUR_HOME_BUTTON_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  BubbleStep(ElementSpecifier(kSearchBoxViewElementId),
                             ContextMode::kAny,
                             HelpBubbleId::kWelcomeTourSearchBox,
                             IDS_ASH_WELCOME_TOUR_SEARCH_BOX_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kSearchBoxViewElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/false),
                  BubbleStep(ElementSpecifier(kSettingsAppElementId),
                             ContextMode::kFromPreviousStep,
                             HelpBubbleId::kWelcomeTourSettingsApp,
                             IDS_ASH_WELCOME_TOUR_SETTINGS_APP_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/true),
                  EventStep(ElementSpecifier(kSettingsAppElementId),
                            ContextMode::kFromPreviousStep,
                            /*has_name_elements_callback=*/false),
                  BubbleStep(ElementSpecifier(kExploreAppElementId),
                             ContextMode::kFromPreviousStep,
                             HelpBubbleId::kWelcomeTourExploreApp,
                             IDS_ASH_WELCOME_TOUR_EXPLORE_APP_BUBBLE_BODY_TEXT,
                             HelpBubbleArrow::kTopRight,
                             /*has_next_button=*/false))))));
}

// Verifies that the Welcome Tour is started when the primary user session is
// first activated and then never again, as well as that start/end events are
// propagated to observers appropriately.
TEST_F(WelcomeTourControllerTest, StartsTourAndPropagatesEvents) {
  const AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  const AccountId secondary_account_id =
      AccountId::FromUserEmail("secondary@test");

  // Ensure controller exists.
  auto* const welcome_tour_controller = WelcomeTourController::Get();
  ASSERT_TRUE(welcome_tour_controller);

  // Ensure delegate exists and disallow any unexpected tutorial starts.
  auto* const user_education_delegate = this->user_education_delegate();
  ASSERT_TRUE(user_education_delegate);
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Observe the `WelcomeTourController` for start/end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(welcome_tour_controller);

  // Add a primary and secondary user session. This should *not* trigger the
  // Welcome Tour to start.
  auto* const session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());

  // Activate the primary user session. This *should* trigger the Welcome Tour
  // to start as well as notify observers. Note that completed/aborted
  // callbacks are cached for later verification.
  base::OnceClosure completed_callback;
  base::OnceClosure aborted_callback;
  EXPECT_CALL(
      *user_education_delegate,
      StartTutorial(Eq(primary_account_id),
                    Eq(TutorialId::kWelcomeTourPrototype1),
                    Eq(welcome_tour_controller->GetInitialElementContext()),
                    /*completed_callback=*/_,
                    /*aborted_callback=*/_))
      .WillOnce(MoveArgs<3, 4>(&completed_callback, &aborted_callback));
  EXPECT_CALL(observer, OnWelcomeTourStarted);
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  Mock::VerifyAndClearExpectations(user_education_delegate);
  Mock::VerifyAndClearExpectations(&observer);

  // The Welcome Tour dialog is expected to be shown at the start of the tour.
  EXPECT_TRUE(WelcomeTourDialog::Get());

  // Disallow any unexpected tutorial starts.
  EXPECT_CALL(*user_education_delegate, StartTutorial).Times(0);

  // Switch to the secondary user session and back again. This should *not*
  // trigger the Welcome Tour to start.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  session_controller_client->SwitchActiveUser(primary_account_id);

  // Deactivate and then reactivate the primary user session. This should *not*
  // trigger the Welcome Tour to start.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  session_controller_client->SetSessionState(SessionState::ACTIVE);

  // Verify that the same event is propagated to observers regardless of whether
  // user education services in the browser indicate the tour was completed or
  // aborted. Only if the tour is completed should there be an attempt to launch
  // the Explore app.
  for (base::OnceClosure& ended_callback :
       {std::ref(completed_callback), std::ref(aborted_callback)}) {
    ASSERT_FALSE(ended_callback.is_null());
    EXPECT_CALL(observer, OnWelcomeTourEnded);
    EXPECT_CALL(*user_education_delegate,
                LaunchSystemWebAppAsync(
                    Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
                    Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())))
        .Times(&ended_callback == &completed_callback ? 1 : 0);
    std::move(ended_callback).Run();
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(user_education_delegate);

    // Aborting the Welcome Tour should close the dialog.
    if (&ended_callback == &aborted_callback) {
      WidgetDestroyedWaiter(WelcomeTourDialog::Get()->GetWidget()).Wait();
      EXPECT_FALSE(WelcomeTourDialog::Get());
    }
  }
}

// Verifies that the Welcome Tour can be aborted via the dialog.
TEST_F(WelcomeTourControllerTest, AbortsTourAndPropagatesEvents) {
  // Expect the Welcome Tour to start when logging in the primary user. Note
  // that the `aborted_callback` is cached.
  base::OnceClosure aborted_callback;
  EXPECT_CALL(*user_education_delegate(),
              StartTutorial(_, Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
      .WillOnce(MoveArg<4>(&aborted_callback));

  // Start the Welcome Tour by logging in the primary user.
  SimulateUserLogin("primary@test");

  // Observe the `WelcomeTourController` for end events.
  StrictMock<MockWelcomeTourControllerObserver> observer;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation{&observer};
  observation.Observe(WelcomeTourController::Get());

  // Satisfy `ended_future` when an end event is received.
  base::test::TestFuture<void> ended_future;
  EXPECT_CALL(observer, OnWelcomeTourEnded)
      .WillOnce(RunOnceClosure(ended_future.GetCallback()));

  // Expect the Welcome Tour to be aborted when clicking the `cancel_button`.
  // Fulfill the request to abort the tour by running the `aborted_callback`.
  EXPECT_CALL(*user_education_delegate(),
              AbortTutorial(_, Eq(TutorialId::kWelcomeTourPrototype1)))
      .WillOnce(RunOnceClosure(std::move(aborted_callback)));

  // Click the `cancel_button` and verify the Welcome Tour is ended.
  const views::View* const cancel_button = GetDialogCancelButton();
  ASSERT_TRUE(cancel_button);
  LeftClickOn(cancel_button);
  EXPECT_TRUE(ended_future.Wait());
}

// WelcomeTourControllerRunTest ------------------------------------------------

// Base class for tests of the `WelcomeTourController` that run the Welcome
// Tour in order to assert expectations before, during, and/or after run time.
class WelcomeTourControllerRunTest : public WelcomeTourControllerTest {
 public:
  // Runs the Welcome Tour, invoking the specified `in_progress_callback` just
  // after the Welcome Tour has started. Note that this method will not return
  // until the Welcome Tour has ended.
  void Run(base::OnceClosure in_progress_callback) {
    // Ensure `controller` exists.
    auto* const controller = WelcomeTourController::Get();
    ASSERT_TRUE(controller);

    // Ensure `delegate` exists.
    auto* const delegate = user_education_delegate();
    ASSERT_TRUE(delegate);

    // Observe the `controller` for Welcome Tour start/end events.
    StrictMock<MockWelcomeTourControllerObserver> observer;
    base::ScopedObservation<WelcomeTourController,
                            WelcomeTourControllerObserver>
        observation{&observer};
    observation.Observe(controller);

    // When the Welcome Tour starts/ends, signal the appropriate future.
    base::test::TestFuture<void> started_future;
    base::test::TestFuture<void> ended_future;
    EXPECT_CALL(observer, OnWelcomeTourStarted)
        .WillOnce(RunOnceClosure(started_future.GetCallback()));
    EXPECT_CALL(observer, OnWelcomeTourEnded)
        .WillOnce(RunOnceClosure(ended_future.GetCallback()));

    // When the Welcome Tour tutorial is started, cache the callback to invoke
    // to complete the tutorial.
    base::OnceClosure completed_callback;
    EXPECT_CALL(
        *delegate,
        StartTutorial(_, Eq(TutorialId::kWelcomeTourPrototype1), _, _, _))
        .WillOnce(MoveArg<3>(&completed_callback));

    // Simulate login of the primary user. Note that this should trigger the
    // Welcome Tour to start automatically.
    const auto primary_account_id = AccountId::FromUserEmail("primary@test");
    SimulateUserLogin(primary_account_id);
    EXPECT_TRUE(started_future.Wait());

    // Invoke the `in_progress_callback` so that tests can assert expectations
    // while the Welcome Tour is in progress.
    std::move(in_progress_callback).Run();

    // When the tour is completed, expect an attempt to launch the Explore app.
    EXPECT_CALL(
        *user_education_delegate(),
        LaunchSystemWebAppAsync(
            Eq(primary_account_id), Eq(ash::SystemWebAppType::HELP),
            Eq(display::Screen::GetScreen()->GetPrimaryDisplay().id())));

    // Click `accept_button` to close the Welcome Tour dialog.
    const views::View* const accept_button = GetDialogAcceptButton();
    ASSERT_TRUE(accept_button);
    LeftClickOn(accept_button);

    // Complete the tutorial by invoking the cached callback.
    std::move(completed_callback).Run();
    EXPECT_TRUE(ended_future.Wait());
    Mock::VerifyAndClearExpectations(user_education_delegate());
  }
};

// Tests -----------------------------------------------------------------------

TEST_F(WelcomeTourControllerRunTest, BlockInteractionsWithIrrelevantWindow) {
  // Create a random widget to interact with.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder()
          .SetBounds(gfx::Rect(100, 100))
          .SetParent(Shell::GetPrimaryRootWindow()->GetChildById(
              kShellWindowId_LockScreenContainer))
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildOwnsNativeWidget();
  MockView* const contents_view =
      widget->SetContentsView(std::make_unique<MockView>());
  widget->GetFocusManager()->SetFocusedView(contents_view);

  // `contents_view` should be interactive before the Welcome Tour.
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_ENTERED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_EXITED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_RELEASED))));
  LeftClickOn(contents_view);
  PressAndReleaseKey(ui::VKEY_A);

  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        // During the Welcome Tour, `contents_view` should NOT be interactive.
        EXPECT_CALL(*contents_view, OnEvent).Times(0);
        LeftClickOn(contents_view);
        PressAndReleaseKey(ui::VKEY_A);
      })));

  // Perform the same set of actions after the Welcome Tour. `contents_view`
  // should be interactive.
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_ENTERED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_MOUSE_EXITED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_PRESSED))));
  EXPECT_CALL(*contents_view,
              OnEvent(Property(&ui::Event::type, Eq(ui::ET_KEY_RELEASED))));
  LeftClickOn(contents_view);
  PressAndReleaseKey(ui::VKEY_A);
}

// Verifies that notifications are blocked during and only during the Welcome
// Tour.
TEST_F(WelcomeTourControllerRunTest, NotificationBlocking) {
  constexpr char kSystemPriorityId[] = "system";
  auto* message_center = message_center::MessageCenter::Get();

  // Case: Before Welcome Tour.
  // Most notifications should be muted, with few exceptions, because no user is
  // logged in.
  {
    SCOPED_TRACE("Initial state");
    ExpectNotificationCounts(/*total_notifications=*/0u,
                             /*visible_notifications=*/0u,
                             /*popup_notifications=*/0u);
  }

  {
    SCOPED_TRACE("Simple notification before login");
    AddNotification("test1");
    ExpectNotificationCounts(
        /*total_notifications=*/1u,
        /*visible_notifications=*/0u,
        /*popup_notifications=*/0u);
  }

  {
    SCOPED_TRACE("Battery notification before login");

    // Use the id from `BatteryNotification` to confirm that those notifications
    // are not muted, as battery notifications show even during OOBE. Note that
    // it does not have system priority; the OOBE exception is based on its id.
    AddNotification(BatteryNotification::kNotificationId);
    ExpectNotificationCounts(
        /*total_notifications=*/2u,
        /*visible_notifications=*/1u,
        /*popup_notifications=*/1u);
  }

  // Case: During Welcome Tour.
  // Notification blocking should hide notifications, including hiding any
  // existing popups.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting([&]() {
        {
          SCOPED_TRACE("Beginning of Welcome Tour");
          ExpectNotificationCounts(
              /*total_notifications=*/2u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }

        {
          SCOPED_TRACE("Simple notification during Welcome Tour");
          AddNotification("test2");
          ExpectNotificationCounts(
              /*total_notifications=*/3u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }

        {
          SCOPED_TRACE("System priority notification during Welcome Tour");

          // System priority notifications should show a popup after the tour is
          // over.
          AddNotification(kSystemPriorityId, /*is_system_priority=*/true);
          ExpectNotificationCounts(
              /*total_notifications=*/4u,
              /*visible_notifications=*/0u,
              /*popup_notifications=*/0u);
        }
      })));

  // Case: After Welcome Tour.
  // All notifications should now be shown in the list. Notifications that
  // happened before or during the tour should not have popups, but new
  // notifications should.
  {
    SCOPED_TRACE("Just after Welcome Tour");
    ExpectNotificationCounts(
        /*total_notifications=*/4u,
        /*visible_notifications=*/4u,
        /*popup_notifications=*/1u);
  }

  // Confirm that the one popup showing is the one with system priority.
  EXPECT_THAT(message_center->GetPopupNotifications(),
              ElementsAre(Property(&message_center::Notification::id,
                                   Eq(kSystemPriorityId))));

  {
    SCOPED_TRACE("Notification after Welcome Tour");
    AddNotification("test3");
    ExpectNotificationCounts(
        /*total_notifications=*/5u,
        /*visible_notifications=*/5u,
        /*popup_notifications=*/2u);
  }
}

// Verifies that scrims are added to all root windows only while the Welcome
// Tour is in progress.
TEST_F(WelcomeTourControllerRunTest, Scrim) {
  // Case: Before Welcome Tour.
  ExpectScrimsOnAllRootWindows(false);

  // Case: During Welcome Tour.
  ASSERT_NO_FATAL_FAILURE(
      Run(/*in_progress_callback=*/base::BindLambdaForTesting(
          [&]() { ExpectScrimsOnAllRootWindows(true); })));

  // Case: After Welcome Tour.
  ExpectScrimsOnAllRootWindows(false);
}

// WelcomeTourControllerTabletTest ---------------------------------------------

// Base class for tests of the `WelcomeTourController` that verify the tour does
// not start/will abort in tablet mode.
class WelcomeTourControllerTabletTest : public WelcomeTourControllerTest {
 public:
  WelcomeTourController* controller() { return WelcomeTourController::Get(); }
  StrictMock<MockWelcomeTourControllerObserver>* observer() {
    return &observer_;
  }

  // WelcomeTourControllerTest:
  void SetUp() override {
    WelcomeTourControllerTest::SetUp();

    // Observe the `WelcomeTourController` for start/end events.
    observation_.Observe(controller());
  }

  void TearDown() override {
    observation_.Reset();

    WelcomeTourControllerTest::TearDown();
  }

 private:
  StrictMock<MockWelcomeTourControllerObserver> observer_;
  base::ScopedObservation<WelcomeTourController, WelcomeTourControllerObserver>
      observation_{&observer_};
};

// Tests -----------------------------------------------------------------------

// Verifies that the Welcome Tour will not start when in tablet mode.
TEST_F(WelcomeTourControllerTabletTest, DoesNotStart) {
  // Force tablet mode on.
  TabletMode::Get()->SetEnabledForTest(true);

  // Activate the primary user session. Since tablet mode is enabled, the
  // Welcome Tour should not start, the dialog should not show, and no start or
  // end calls should be made.
  EXPECT_CALL(*user_education_delegate(), StartTutorial).Times(0);
  SimulateUserLogin("user@test");
  EXPECT_FALSE(WelcomeTourDialog::Get());
  Mock::VerifyAndClearExpectations(user_education_delegate());
  Mock::VerifyAndClearExpectations(observer());
}

// Verifies that the tour will abort if we enter tablet mode.
TEST_F(WelcomeTourControllerTabletTest, TriggersAbort) {
  // Activate the user session to trigger the Welcome Tour to start, as well as
  // notify observers. Note that the aborted callback is cached for later
  // verification.
  base::OnceClosure aborted_callback;
  EXPECT_CALL(
      *user_education_delegate(),
      StartTutorial(/*account_id=*/_, Eq(TutorialId::kWelcomeTourPrototype1),
                    /*element_context=*/_,
                    /*completed_callback=*/_,
                    /*aborted_callback=*/_))
      .WillOnce(MoveArgs<4>(&aborted_callback));
  EXPECT_CALL(*observer(), OnWelcomeTourStarted);
  SimulateUserLogin("user@test");
  Mock::VerifyAndClearExpectations(user_education_delegate());
  Mock::VerifyAndClearExpectations(observer());
  ASSERT_FALSE(aborted_callback.is_null());

  // Force tablet mode on, which should cause the tutorial to abort.
  EXPECT_CALL(*user_education_delegate(), AbortTutorial)
      .WillOnce(RunOnceClosure(std::move(aborted_callback)));
  EXPECT_CALL(*observer(), OnWelcomeTourEnded);
  TabletMode::Get()->SetEnabledForTest(true);
  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(user_education_delegate());

  // Wait for the dialog widget to be destroyed.
  ASSERT_THAT(WelcomeTourDialog::Get(),
              Property(&WelcomeTourDialog::GetWidget, NotNull()));
  WidgetDestroyedWaiter(WelcomeTourDialog::Get()->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

}  // namespace ash
