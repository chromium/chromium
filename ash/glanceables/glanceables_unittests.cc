// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_restore_view.h"
#include "ash/glanceables/glanceables_up_next_event_item_view.h"
#include "ash/glanceables/glanceables_up_next_view.h"
#include "ash/glanceables/glanceables_util.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/glanceables/signout_screenshot_handler.h"
#include "ash/glanceables/test_glanceables_delegate.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"

namespace ash {
namespace {

// A SignoutScreenshotHandler that skips taking the screenshot and invokes its
// done callback immediately.
class TestSignoutScreenshotHandler : public SignoutScreenshotHandler {
 public:
  // SignoutScreenshotHandler:
  void TakeScreenshot(base::OnceClosure done_callback) override {
    ++take_screenshot_count_;
    std::move(done_callback).Run();
  }

  int take_screenshot_count_ = 0;
};

AmbientWeatherModel* GetWeatherModel() {
  return Shell::Get()->ambient_controller()->GetAmbientWeatherModel();
}

}  // namespace

// Unified test suite for the glanceables controller, views, etc.
class GlanceablesTest : public AshTestBase {
 public:
  GlanceablesTest() = default;
  ~GlanceablesTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->glanceables_controller();
    DCHECK(controller_);

    // Fake out the ambient backend controller so weather fetches won't crash.
    auto* ambient_controller = Shell::Get()->ambient_controller();
    // The controller must be null before a new instance can be created.
    ambient_controller->set_backend_controller_for_testing(nullptr);
    ambient_controller->set_backend_controller_for_testing(
        std::make_unique<FakeAmbientBackendControllerImpl>());
  }

  google_apis::calendar::CalendarEvent CreateTestEvent() {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString("11 Jan 2022 18:00 GMT", &start_time));
    google_apis::calendar::DateTime start_date_time;
    start_date_time.set_date_time(start_time);

    google_apis::calendar::CalendarEvent event;
    event.set_summary("Test event 123");
    event.set_start_time(start_date_time);
    event.set_html_link("https://www.google.com/calendar/event?eid=qwerty");
    return event;
  }

  // Event summaries in this method are valid for now == 10 Jan 2022 13:00 GMT.
  void SimulateCalendarEventsFetched() {
    auto fetched_events = std::make_unique<google_apis::calendar::EventList>();
    fetched_events->set_time_zone("Greenwich Mean Time");
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_0", "Past event, the day before", "9 Jan 2022 8:30 GMT",
        "9 Jan 2022 9:30 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_1", "Future event, the day after", "11 Jan 2022 18:00 GMT",
        "11 Jan 2022 18:45 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_2", "Past event, today", "10 Jan 2022 10:00 GMT",
        "10 Jan 2022 11:00 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_3", "Ongoing event, started >1.5hrs ago", "10 Jan 2022 10:00 GMT",
        "10 Jan 2022 14:00 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_4", "Future event, later today", "10 Jan 2022 21:30 GMT",
        "10 Jan 2022 22:30 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_5", "Ongoing event, started <1.5hrs ago (xyz)",
        "10 Jan 2022 12:00 GMT", "10 Jan 2022 14:00 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_6", "All-day event", "10 Jan 2022 21:00 GMT",
        "11 Jan 2022 21:00 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_7", "Ongoing event, started <1.5hrs ago (abc)",
        "10 Jan 2022 12:00 GMT", "10 Jan 2022 14:00 GMT"));
    fetched_events->InjectItemForTesting(calendar_test_utils::CreateEvent(
        "id_8", "Future event, later today (same start time, but longer)",
        "10 Jan 2022 21:30 GMT", "10 Jan 2022 22:40 GMT"));
    Shell::Get()->system_tray_model()->calendar_model()->OnEventsFetched(
        calendar_utils::GetStartOfMonthUTC(base::Time::Now()),
        google_apis::ApiErrorCode::HTTP_SUCCESS, fetched_events.get());
  }

  TestGlanceablesDelegate* GetTestDelegate() {
    return static_cast<TestGlanceablesDelegate*>(controller_->delegate_.get());
  }

  views::Widget* GetWidget() { return controller_->widget_.get(); }

  GlanceablesView* GetGlanceablesView() { return controller_->view_; }

  GlanceablesWelcomeLabel* GetWelcomeLabel() {
    return controller_->view_->welcome_label_;
  }

  views::ImageView* GetWeatherIcon() {
    return controller_->view_->weather_view_->icon_;
  }

  views::Label* GetWeatherTemperature() {
    return controller_->view_->weather_view_->temperature_;
  }

  GlanceablesUpNextView* GetUpNextView() {
    return controller_->view_->up_next_view_;
  }

  std::vector<GlanceablesUpNextEventItemView*> GetEventItemViews() {
    return GetUpNextView()->event_item_views_;
  }

  views::Label* GetNoEventsLabel() { return GetUpNextView()->no_events_label_; }

  views::Label* GetRestoreSessionLabel() {
    return controller_->view_->restore_session_label_;
  }

  GlanceablesRestoreView* GetRestoreView() {
    return controller_->view_->restore_view_;
  }

  views::ImageButton* GetRestoreViewImageButton() {
    return GetRestoreView()->image_button_;
  }

  PillButton* GetRestoreViewPillButton() {
    return GetRestoreView()->pill_button_;
  }

 protected:
  GlanceablesController* controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesTest, CreateAndDestroyUi) {
  ASSERT_EQ(0, GetTestDelegate()->on_glanceables_closed_count());

  controller_->CreateUi();

  // A fullscreen widget was created.
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsFullscreen());

  // The controller's view is the widget's contents view.
  views::View* view = GetGlanceablesView();
  EXPECT_TRUE(view);
  EXPECT_EQ(view, widget->GetContentsView());

  // Backdrop was applied.
  EXPECT_GT(GetWidget()->GetLayer()->background_blur(), 0);
  EXPECT_TRUE(view->GetBackground());

  controller_->DestroyUi();

  // Widget and glanceables view are destroyed.
  EXPECT_FALSE(GetWidget());
  EXPECT_FALSE(GetGlanceablesView());

  // Delegate was notified that glanceables were closed.
  EXPECT_EQ(1, GetTestDelegate()->on_glanceables_closed_count());
}

TEST_F(GlanceablesTest, HidesInTabletMode) {
  controller_->CreateUi();
  ASSERT_TRUE(controller_->IsShowing());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(controller_->IsShowing());
}

TEST_F(GlanceablesTest, GlanceablesViewCreatesChildViews) {
  controller_->CreateUi();

  GlanceablesView* view = GetGlanceablesView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(GetWelcomeLabel());
  EXPECT_TRUE(GetWeatherIcon());
  EXPECT_TRUE(GetWeatherTemperature());
  EXPECT_TRUE(GetUpNextView());
  EXPECT_TRUE(GetRestoreSessionLabel());
  EXPECT_TRUE(GetRestoreView());
}

TEST_F(GlanceablesTest, ShowFromOverviewDoesNotCreateRestoreViews) {
  controller_->ShowFromOverview();

  GlanceablesView* view = GetGlanceablesView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(GetWelcomeLabel());
  EXPECT_TRUE(GetWeatherIcon());
  EXPECT_TRUE(GetWeatherTemperature());
  EXPECT_TRUE(GetUpNextView());

  // Session restore views are skipped.
  EXPECT_FALSE(GetRestoreSessionLabel());
  EXPECT_FALSE(GetRestoreView());
}

TEST_F(GlanceablesTest, WeatherViewShowsWeather) {
  controller_->CreateUi();

  // Icon starts blank.
  views::ImageView* icon = GetWeatherIcon();
  EXPECT_TRUE(icon->GetImage().isNull());

  // Trigger a weather update. Use an image the same size as the icon view's
  // image so the image won't be resized and we can compare backing objects.
  gfx::Rect image_bounds = icon->GetImageBounds();
  gfx::ImageSkia weather_image =
      gfx::test::CreateImageSkia(image_bounds.width(), image_bounds.height());
  GetWeatherModel()->UpdateWeatherInfo(weather_image, 72.0f,
                                       /*show_celsius=*/false);

  // The view reflects the new weather.
  EXPECT_EQ(weather_image.GetBackingObject(),
            icon->GetImage().GetBackingObject());
  EXPECT_EQ(u"72° F", GetWeatherTemperature()->GetText());
}

TEST_F(GlanceablesTest, UpNextViewFiltersAndSortsEvents) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time now;
        EXPECT_TRUE(base::Time::FromString("10 Jan 2022 13:00 GMT", &now));
        return now;
      },
      nullptr, nullptr);

  controller_->CreateUi();
  SimulateCalendarEventsFetched();

  // Events list contains rendered event items inside.
  const auto& items = GetEventItemViews();
  EXPECT_EQ(items.size(), 4u);

  EXPECT_EQ(items[0]->event_title_label_for_test()->GetText(),
            u"Ongoing event, started <1.5hrs ago (abc)");
  EXPECT_EQ(items[1]->event_title_label_for_test()->GetText(),
            u"Ongoing event, started <1.5hrs ago (xyz)");
  EXPECT_EQ(items[2]->event_title_label_for_test()->GetText(),
            u"Future event, later today (same start time, but longer)");
  EXPECT_EQ(items[3]->event_title_label_for_test()->GetText(),
            u"Future event, later today");
}

TEST_F(GlanceablesTest, UpNextViewShowsNoEventsLabel) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time now;
        // `SimulateCalendarEventsFetched()` call below won't have any events
        // for this date/time.
        EXPECT_TRUE(base::Time::FromString("12 Jan 2022 13:00 GMT", &now));
        return now;
      },
      nullptr, nullptr);

  controller_->CreateUi();
  SimulateCalendarEventsFetched();

  EXPECT_EQ(GetEventItemViews().size(), 0u);
  EXPECT_TRUE(GetNoEventsLabel());
  EXPECT_EQ(GetNoEventsLabel()->GetText(), u"No events today");
}

TEST_F(GlanceablesTest, UpNextEventItemViewOpensCalendarEvent) {
  GlanceablesUpNextEventItemView view(CreateTestEvent());

  EXPECT_EQ(GetSystemTrayClient()->show_calendar_event_count(), 0);
  view.AcceleratorPressed(ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, 0));
  EXPECT_EQ(GetSystemTrayClient()->show_calendar_event_count(), 1);
}

TEST_F(GlanceablesTest, UpNextEventItemViewRendersCorrectlyIn12HrClockFormat) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  GlanceablesUpNextEventItemView view(CreateTestEvent());

  EXPECT_EQ(view.event_title_label_for_test()->GetText(), u"Test event 123");
  EXPECT_EQ(view.event_time_label_for_test()->GetText(), u"6:00\u202fPM");
}

TEST_F(GlanceablesTest, UpNextEventItemViewRendersCorrectlyIn24HrClockFormat) {
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");
  GlanceablesUpNextEventItemView view(CreateTestEvent());

  EXPECT_EQ(view.event_title_label_for_test()->GetText(), u"Test event 123");
  EXPECT_EQ(view.event_time_label_for_test()->GetText(), u"18:00");
}

TEST_F(GlanceablesTest, UpNextEventItemViewRendersCorrectlyWithoutEventTitle) {
  google_apis::calendar::CalendarEvent event;
  GlanceablesUpNextEventItemView view(event);

  EXPECT_EQ(view.GetAccessibleName(), u"(No title)");
  EXPECT_EQ(view.event_title_label_for_test()->GetText(), u"(No title)");
}

TEST_F(GlanceablesTest, RestoreViewRendersScreenshot) {
  data_decoder::test::InProcessDataDecoder data_decoder;
  const SkColor expected_color = SK_ColorYELLOW;

  // Override home directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedPathOverride home_dir_override(base::DIR_HOME,
                                             temp_dir.GetPath());

  // Simulate that shutdown screenshot is there.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(400, 300);
  bitmap.eraseColor(expected_color);
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data);
  ASSERT_TRUE(base::WriteFile(
      temp_dir.GetPath().AppendASCII("signout_screenshot.png"), png_data));

  controller_->CreateUi();
  GlanceablesRestoreView* restore_view = GetRestoreView();
  ASSERT_TRUE(restore_view);

  // Wait for GlanceablesRestoreView `image_util::DecodeImageFile` callback.
  base::RunLoop().RunUntilIdle();
  views::ImageButton* image_button = GetRestoreViewImageButton();
  ASSERT_TRUE(image_button);
  ASSERT_FALSE(GetRestoreViewPillButton());
  gfx::ImageSkia image = image_button->GetImage(views::Button::STATE_NORMAL);
  EXPECT_FALSE(image.isNull());
  EXPECT_GT(image.width(), 0);
  EXPECT_GT(image.height(), 0);
  EXPECT_EQ(image.bitmap()->getColor(150, 100), expected_color);
}

TEST_F(GlanceablesTest, ClickOnSessionRestore) {
  controller_->CreateUi();
  GlanceablesRestoreView* restore_view = GetRestoreView();
  ASSERT_TRUE(restore_view);

  // Wait for GlanceablesRestoreView `image_util::DecodeImageFile` callback.
  base::RunLoop().RunUntilIdle();

  PillButton* restore_button = GetRestoreViewPillButton();
  ASSERT_TRUE(restore_button);
  ASSERT_EQ(0, GetTestDelegate()->restore_session_count());

  // Click on the "Restore" button.
  views::test::ButtonTestApi(restore_button).NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(1, GetTestDelegate()->restore_session_count());
  EXPECT_FALSE(controller_->IsShowing());
}

TEST_F(GlanceablesTest, DismissesOnlyOnAppWindowOpen) {
  controller_->CreateUi();
  ASSERT_TRUE(controller_->IsShowing());

  // Showing the app list still shows glanceables.
  GetAppListTestHelper()->ShowAppList();
  EXPECT_TRUE(controller_->IsShowing());

  // Showing quick settings still shows glanceables.
  UnifiedSystemTray* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  tray->ActivateBubble();
  EXPECT_TRUE(controller_->IsShowing());

  // Creating an app window hides glanceables.
  std::unique_ptr<aura::Window> app_window = CreateAppWindow();
  EXPECT_FALSE(controller_->IsShowing());

  // Glanceables stay hidden after the app window is closed.
  app_window.reset();
  EXPECT_FALSE(controller_->IsShowing());
}

TEST_F(GlanceablesTest, ShowFromOverview) {
  ASSERT_FALSE(controller_->IsShowing());

  EnterOverview();
  const DesksBarView* desks_bar_view = GetPrimaryRootDesksBarView();
  auto* up_next_button = desks_bar_view->up_next_button();
  ASSERT_TRUE(up_next_button);

  LeftClickOn(up_next_button);

  // Glanceables are showing and overview mode is closed.
  EXPECT_TRUE(controller_->IsShowing());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(GlanceablesTest, OverviewDoesNotHaveUpNextButtonForSecondaryUser) {
  // Sign in a secondary user.
  SimulateUserLogin("user@test.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserPrimary());

  // Overview mode does not have the "Up next" button.
  EnterOverview();
  const DesksBarView* desks_bar_view = GetPrimaryRootDesksBarView();
  EXPECT_FALSE(desks_bar_view->up_next_button());
}

TEST_F(GlanceablesTest, ShowFromOverviewHidesAppWindows) {
  // Create windows, back to front.
  std::unique_ptr<aura::Window> back_window = CreateAppWindow();
  std::unique_ptr<aura::Window> middle_window = CreateAppWindow();
  std::unique_ptr<aura::Window> minimized_window = CreateAppWindow();
  WindowState::Get(minimized_window.get())->Minimize();
  std::unique_ptr<aura::Window> front_window = CreateAppWindow();

  controller_->ShowFromOverview();

  // All windows are minimized.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(middle_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(minimized_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsMinimized());

  // Destroy the middle window.
  middle_window.reset();

  // Hide glanceables.
  controller_->DestroyUi();

  // Front and back windows are restored.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsNormalStateType());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsNormalStateType());

  // The originally minimized window is still minimized.
  EXPECT_TRUE(WindowState::Get(minimized_window.get())->IsMinimized());

  // The front window is still frontmost (at the end of the child list).
  EXPECT_EQ(front_window->parent()->children().back(), front_window.get());
}

TEST_F(GlanceablesTest, UnminimizingOneWindowRestoresAllWindows) {
  std::unique_ptr<aura::Window> back_window = CreateAppWindow();
  std::unique_ptr<aura::Window> front_window = CreateAppWindow();

  controller_->ShowFromOverview();

  EXPECT_TRUE(WindowState::Get(back_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsMinimized());

  // Restore and activate the front window.
  WindowState::Get(front_window.get())->Unminimize();
  WindowState::Get(front_window.get())->Activate();

  // Window activation closed glanceables.
  EXPECT_FALSE(controller_->IsShowing());

  // Both windows are restored.
  EXPECT_TRUE(WindowState::Get(back_window.get())->IsNormalStateType());
  EXPECT_TRUE(WindowState::Get(front_window.get())->IsNormalStateType());
}

TEST_F(GlanceablesTest, RequestRestartForUpdateTakesScreenshot) {
  GetTestDelegate()->set_should_take_signout_screenshot(true);

  auto* session_controller = Shell::Get()->session_controller();
  auto screenshot_handler = std::make_unique<TestSignoutScreenshotHandler>();
  auto* screenshot_handler_ptr = screenshot_handler.get();
  session_controller->SetSignoutScreenshotHandlerForTest(
      std::move(screenshot_handler));

  session_controller->RequestRestartForUpdate();

  // Screenshot was taken.
  EXPECT_EQ(1, screenshot_handler_ptr->take_screenshot_count_);

  // Restart was requested.
  EXPECT_EQ(1,
            GetSessionControllerClient()->request_restart_for_update_count());
}

TEST_F(GlanceablesTest, RecordSignoutScreenshotDurationMetric) {
  PrefService* local_state = Shell::Get()->local_state();

  // Simulate a previous session that recorded a duration.
  const base::TimeDelta duration = base::Milliseconds(123);
  glanceables_util::SaveSignoutScreenshotDuration(local_state, duration);

  // Recording the metric records a histogram.
  base::HistogramTester histograms;
  glanceables_util::RecordSignoutScreenshotDurationMetric(local_state);
  histograms.ExpectUniqueTimeSample("Ash.Glanceables.SignoutScreenshotDuration",
                                    duration, 1);

  // Pref is reset.
  const base::TimeDelta updated_duration =
      glanceables_util::GetSignoutScreenshotDurationForTest(local_state);
  EXPECT_EQ(0, updated_duration.InMilliseconds());
}

}  // namespace ash
