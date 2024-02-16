// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/wm/desks/templates/admin_template_launch_tracker.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/overview/overview_test_base.h"
#include "base/json/json_string_value_serializer.h"
#include "base/uuid.h"
#include "components/app_restore/restore_data.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Le;
using testing::Not;
using testing::Optional;

class MockSavedDeskDelegate : public SavedDeskDelegate {
 public:
  MOCK_METHOD(void,
              GetAppLaunchDataForSavedDesk,
              (aura::Window*, GetAppLaunchDataCallback),
              (const override));
  MOCK_METHOD(desks_storage::DeskModel*, GetDeskModel, (), (override));
  MOCK_METHOD(desks_storage::AdminTemplateService*,
              GetAdminTemplateService,
              (),
              (override));
  MOCK_METHOD(bool, IsWindowPersistable, (aura::Window*), (const override));
  MOCK_METHOD(std::optional<gfx::ImageSkia>,
              MaybeRetrieveIconForSpecialIdentifier,
              (const std::string&, const ui::ColorProvider*),
              (const override));
  MOCK_METHOD(void,
              GetFaviconForUrl,
              (const std::string&,
               uint64_t,
               base::OnceCallback<void(const gfx::ImageSkia&)>,
               base::CancelableTaskTracker*),
              (const override));
  MOCK_METHOD(void,
              GetIconForAppId,
              (const std::string&,
               int,
               base::OnceCallback<void(const gfx::ImageSkia&)>),
              (const override));
  MOCK_METHOD(void,
              LaunchAppsFromSavedDesk,
              (std::unique_ptr<DeskTemplate>),
              (override));
  MOCK_METHOD(bool,
              IsWindowSupportedForSavedDesk,
              (aura::Window*),
              (const override));
  MOCK_METHOD(std::string, GetAppShortName, (const std::string&), (override));
  MOCK_METHOD(bool, IsAppAvailable, (const std::string&), (const override));
};

constexpr char kAdminTemplateJson[] = R"json({
   "mgndgikekgjfcpckkfioiadnlibdjbkf": {
      "1": {
         "active_tab_index": 0,
         "app_name": "",
         "current_bounds": [ 100, 50, 640, 480 ],
         "index": 0,
         "urls": [ "https://www.google.com/" ]
      }
   }
})json";

constexpr char kAdminTemplateWithoutBoundsJson[] = R"json({
   "mgndgikekgjfcpckkfioiadnlibdjbkf": {
      "1": {
         "active_tab_index": 0,
         "app_name": "",
         "index": 0,
         "urls": [ "https://www.google.com/" ]
      }
   }
})json";

class AdminTemplateTest : public OverviewTestBase,
                          public testing::WithParamInterface<const char*> {
 public:
  AdminTemplateTest() = default;
  AdminTemplateTest(const AdminTemplateTest&) = delete;
  AdminTemplateTest& operator=(const AdminTemplateTest&) = delete;
  ~AdminTemplateTest() override = default;

 protected:
  // Creates a template from a JSON string.
  std::unique_ptr<DeskTemplate> CreateTemplateFromJson(std::string_view json) {
    base::JSONReader::Result json_read_result =
        base::JSONReader::ReadAndReturnValueWithError(json);
    if (!json_read_result.has_value()) {
      return nullptr;
    }

    auto admin_template = std::make_unique<DeskTemplate>(
        base::Uuid::GenerateRandomV4(), DeskTemplateSource::kPolicy,
        "admin template", base::Time::Now(), DeskTemplateType::kTemplate);
    admin_template->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>(
            std::move(json_read_result).value()));

    return admin_template;
  }
};

TEST_F(AdminTemplateTest, MergeAdminTemplateWindowUpdate) {
  auto admin_template = CreateTemplateFromJson(kAdminTemplateJson);
  ASSERT_TRUE(admin_template);

  const auto* app_restore_data = QueryRestoreData(*admin_template, {});
  ASSERT_TRUE(app_restore_data);

  // Using a window ID not present in the template.
  EXPECT_FALSE(
      MergeAdminTemplateWindowUpdate(*admin_template, {.template_rwid = 123}));

  // Checks that the original window bounds are present.
  EXPECT_THAT(app_restore_data->window_info.current_bounds,
              Optional(gfx::Rect(100, 50, 640, 480)));

  gfx::Rect new_bounds(10, 10, 300, 200);
  EXPECT_TRUE(MergeAdminTemplateWindowUpdate(
      *admin_template, {.template_rwid = 1, .bounds = new_bounds}));
  EXPECT_THAT(app_restore_data->window_info.current_bounds,
              Optional(new_bounds));

  EXPECT_THAT(app_restore_data->display_id, Eq(std::nullopt));
  EXPECT_TRUE(MergeAdminTemplateWindowUpdate(
      *admin_template, {.template_rwid = 1, .display_id = 123456}));
  EXPECT_THAT(app_restore_data->display_id, Optional(123456));
}

TEST_F(AdminTemplateTest, AdjustAdminTemplateWindowBounds) {
  // Simulates 800x600, but with a 50 pixel inset at top.
  const gfx::Rect work_area(0, 50, 800, 550);

  struct TestCase {
    const char* name;
    // The input bounds.
    gfx::Rect bounds;
    // Existing bounds.
    std::vector<gfx::Rect> existing;
    // Expected output bounds.
    gfx::Rect expected;
  };

  const TestCase tests[] = {
      {.name = "Not adjusted",
       .bounds = gfx::Rect(100, 100, 640, 480),
       .existing = {},
       .expected = gfx::Rect(100, 100, 640, 480)},
      {.name = "Adjusted to fit work area",
       .bounds = gfx::Rect(0, 0, 640, 480),
       .existing = {},
       .expected = gfx::Rect(0, 50, 640, 480)},
      {.name = "Existing, but no overlap",
       .bounds = gfx::Rect(100, 100, 640, 480),
       .existing = {gfx::Rect(20, 50, 640, 480)},
       .expected = gfx::Rect(100, 100, 640, 480)},
      {.name = "Existing, with exact overlap",
       .bounds = gfx::Rect(100, 100, 640, 480),
       .existing = {gfx::Rect(100, 100, 640, 480)},
       .expected = gfx::Rect(110, 110, 640, 480)},
      {.name = "Existing, adjusted multiple times",
       .bounds = gfx::Rect(100, 100, 640, 480),
       .existing = {gfx::Rect(100, 100, 640, 480),
                    gfx::Rect(110, 110, 640, 480)},
       .expected = gfx::Rect(120, 120, 640, 480)},
      {.name = "Existing, adjustment clamped in the x direction",
       .bounds = gfx::Rect(155, 100, 640, 480),
       .existing = {gfx::Rect(155, 100, 640, 480)},
       .expected = gfx::Rect(160, 110, 640, 480)},
      {.name = "Existing, adjustment clamped in the y direction",
       .bounds = gfx::Rect(100, 115, 640, 480),
       .existing = {gfx::Rect(100, 115, 640, 480)},
       .expected = gfx::Rect(110, 120, 640, 480)},
      {.name = "Existing, adjustment not possible",
       .bounds = gfx::Rect(160, 120, 640, 480),
       .existing = {gfx::Rect(160, 120, 640, 480)},
       .expected = gfx::Rect(160, 120, 640, 480)}};

  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.name);
    gfx::Rect bounds = test.bounds;
    AdjustAdminTemplateWindowBounds(work_area, test.existing, bounds);
    EXPECT_EQ(bounds, test.expected);
  }
}

// Tests that when we have different number of windows the bounds for each
// window is expected.
TEST_F(AdminTemplateTest, GetInitialWindowLayout) {
  const gfx::Size work_area_size(1000, 800);

  // Verifies the bounds when there is only one window.
  EXPECT_THAT(GetInitialWindowLayout(work_area_size, 1),
              ElementsAre(gfx::Rect(100, 80, 800, 640)));

  // Verifies the bounds when there are two windows.
  EXPECT_THAT(
      GetInitialWindowLayout(work_area_size, 2),
      ElementsAre(gfx::Rect(0, 0, 500, 800), gfx::Rect(500, 0, 500, 800)));

  // Verifies the bounds when there are three windows.
  EXPECT_THAT(
      GetInitialWindowLayout(work_area_size, 3),
      ElementsAre(gfx::Rect(0, 0, 500, 800), gfx::Rect(500, 0, 500, 400),
                  gfx::Rect(500, 400, 500, 400)));

  // Verifies the bounds when there are four windows.
  EXPECT_THAT(
      GetInitialWindowLayout(work_area_size, 4),
      ElementsAre(gfx::Rect(0, 0, 500, 400), gfx::Rect(500, 0, 500, 400),
                  gfx::Rect(0, 400, 500, 400), gfx::Rect(500, 400, 500, 400)));

  // Verifies the bounds when there are eight windows.
  EXPECT_THAT(
      GetInitialWindowLayout(work_area_size, 8),
      ElementsAre(gfx::Rect(0, 0, 500, 400), gfx::Rect(500, 0, 500, 400),
                  gfx::Rect(0, 400, 500, 400), gfx::Rect(500, 400, 500, 400),
                  gfx::Rect(0, 0, 500, 400), gfx::Rect(500, 0, 500, 400),
                  gfx::Rect(0, 400, 500, 400), gfx::Rect(500, 400, 500, 400)));
}

TEST_P(AdminTemplateTest, LaunchTemplate) {
  auto admin_template = CreateTemplateFromJson(GetParam());
  ASSERT_TRUE(admin_template);

  AdminTemplateLaunchTracker launch_tracker(
      std::move(admin_template), base::DoNothing(), base::TimeDelta());

  MockSavedDeskDelegate saved_desk_delegate;
  std::unique_ptr<DeskTemplate> launched_template;

  // Tells the tracker to launch the template. This will modify parts of the
  // template, set up trackers and eventually send it to the delegate. We'll
  // capture the launched template here so that it can be inspected.
  EXPECT_CALL(saved_desk_delegate, LaunchAppsFromSavedDesk(_))
      .WillOnce([&](std::unique_ptr<DeskTemplate> arg) {
        launched_template = std::move(arg);
      });
  launch_tracker.LaunchTemplate(&saved_desk_delegate,
                                /*default_display_id=*/-1);

  ASSERT_TRUE(launched_template);
  const auto* app_restore_data = QueryRestoreData(*launched_template, {});
  ASSERT_TRUE(app_restore_data);

  // Verifies that a display id has been assigned.
  EXPECT_THAT(app_restore_data->display_id, Optional(Not(Eq(-1))));

  // And window activation index.
  EXPECT_THAT(app_restore_data->window_info.activation_index,
              Optional(Le(kTemplateStartingActivationIndex)));
  // Verifies that the window has been assigned with bounds.
  ASSERT_FALSE(app_restore_data->window_info.current_bounds->IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AdminTemplateTest,
                         testing::Values(kAdminTemplateJson,
                                         kAdminTemplateWithoutBoundsJson));

}  // namespace
}  // namespace ash
