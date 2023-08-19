// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/test_widget_builder.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_ping_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Pair;
using ::user_education::TutorialDescription;

// Helpers ---------------------------------------------------------------------

HoldingSpaceTray* GetHoldingSpaceTrayForShelf(Shelf* shelf) {
  return shelf->GetStatusAreaWidget()->holding_space_tray();
}

aura::Window* GetRootWindowForDisplayId(int64_t display_id) {
  return Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
      display_id);
}

Shelf* GetShelfForDisplayId(int64_t display_id) {
  return Shelf::ForWindow(GetRootWindowForDisplayId(display_id));
}

WallpaperView* GetWallpaperViewForDisplayId(int64_t display_id) {
  return RootWindowController::ForWindow(GetRootWindowForDisplayId(display_id))
      ->wallpaper_widget_controller()
      ->wallpaper_view();
}

std::unique_ptr<HoldingSpaceImage> CreateHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

std::unique_ptr<HoldingSpaceItem> CreateHoldingSpaceItem(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return HoldingSpaceItem::CreateFileBackedItem(
      type, HoldingSpaceFile(HoldingSpaceFile::FileSystemType::kTest),
      file_path,
      GURL(base::StrCat({"file-system:", file_path.BaseName().value()})),
      base::BindOnce(&CreateHoldingSpaceImage));
}

std::vector<std::unique_ptr<HoldingSpaceItem>> CreateHoldingSpaceItems(
    HoldingSpaceItem::Type type,
    const std::vector<base::FilePath>& file_paths) {
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  for (const base::FilePath& file_path : file_paths) {
    items.emplace_back(CreateHoldingSpaceItem(type, file_path));
  }
  return items;
}

std::unique_ptr<views::Widget> CreateTestWidgetForDisplayId(
    int64_t display_id) {
  return TestWidgetBuilder()
      .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
      .SetContext(GetRootWindowForDisplayId(display_id))
      .BuildOwnsNativeWidget();
}

bool HasHelpBubble(HoldingSpaceTray* tray) {
  absl::optional<HelpBubbleId> help_bubble_id =
      UserEducationHelpBubbleController::Get()->GetHelpBubbleId(
          kHoldingSpaceTrayElementId,
          views::ElementTrackerViews::GetContextForView(tray));

  // Add failures if the help bubble is not the one that's expected.
  EXPECT_EQ(help_bubble_id.value_or(HelpBubbleId::kHoldingSpaceTour),
            HelpBubbleId::kHoldingSpaceTour);

  return help_bubble_id.has_value();
}

bool HasPing(HoldingSpaceTray* tray) {
  absl::optional<PingId> ping_id =
      UserEducationPingController::Get()->GetPingId(tray);

  // Add failures if the ping is not the one that's expected.
  EXPECT_EQ(ping_id.value_or(PingId::kHoldingSpaceTour),
            PingId::kHoldingSpaceTour);

  return ping_id.has_value();
}

bool HasWallpaperHighlight(int64_t display_id) {
  auto* const wallpaper_view = GetWallpaperViewForDisplayId(display_id);

  bool has_wallpaper_highlight = false;
  bool below_wallpaper_view_layer = true;

  for (const auto* wallpaper_layer : wallpaper_view->GetLayersInOrder()) {
    if (wallpaper_layer == wallpaper_view->layer()) {
      below_wallpaper_view_layer = false;
      continue;
    }

    if (wallpaper_layer->name() !=
        HoldingSpaceTourController::kHighlightLayerName) {
      continue;
    }

    has_wallpaper_highlight = true;

    // Add failures if the highlight layer is not configured as expected.
    EXPECT_FALSE(below_wallpaper_view_layer);
    EXPECT_EQ(wallpaper_layer->type(), ui::LAYER_SOLID_COLOR);
    EXPECT_EQ(wallpaper_layer->bounds(), wallpaper_view->layer()->bounds());
    EXPECT_EQ(wallpaper_layer->background_color(),
              SkColorSetA(wallpaper_view->GetColorProvider()->GetColor(
                              cros_tokens::kCrosSysPrimaryLight),
                          0.4f * SK_AlphaOPAQUE));
  }

  return has_wallpaper_highlight;
}

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void SetFilesAppData(ui::OSExchangeData* data,
                     const std::u16string& file_system_sources) {
  base::Pickle pickled_data;
  ui::WriteCustomDataToPickle(
      std::unordered_map<std::u16string, std::u16string>(
          {{u"fs/sources", file_system_sources}}),
      &pickled_data);

  // NOTE: The Files app stores file system sources as custom web data.
  data->SetPickledData(ui::ClipboardFormatType::WebCustomDataType(),
                       pickled_data);
}

// DraggableView ---------------------------------------------------------------

// TODO(http://b/279211692): Modify and reuse `DraggableTestView`.
// A view supporting drag operations that relies on a `delegate_` to write data.
class DraggableView : public views::View {
 public:
  using Delegate = base::RepeatingCallback<void(ui::OSExchangeData*)>;
  explicit DraggableView(Delegate delegate) : delegate_(std::move(delegate)) {}

 private:
  // views::View:
  int GetDragOperations(const gfx::Point&) override {
    return ui::DragDropTypes::DragOperation::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point&, ui::OSExchangeData* data) override {
    delegate_.Run(data);
  }

  // The delegate for writing drag data.
  Delegate delegate_;
};

}  // namespace

// HoldingSpaceTourControllerTest ----------------------------------------------

// Base class for tests of the `HoldingSpaceTourController`.
class HoldingSpaceTourControllerTest : public UserEducationAshTestBase {
 public:
  HoldingSpaceTourControllerTest() {
    // NOTE: The `HoldingSpaceTourController` exists only when the Holding Space
    // Tour feature is enabled. Controller existence is verified in test
    // coverage for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kHoldingSpaceTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(HoldingSpaceTourControllerTest, GetTutorialDescriptions) {
  auto* holding_space_tour_controller = HoldingSpaceTourController::Get();
  ASSERT_TRUE(holding_space_tour_controller);

  std::map<TutorialId, TutorialDescription> tutorial_descriptions_by_id =
      static_cast<UserEducationFeatureController*>(
          holding_space_tour_controller)
          ->GetTutorialDescriptions();

  // TODO(http://b/275909980): Implement tutorial descriptions.
  EXPECT_THAT(
      tutorial_descriptions_by_id,
      ElementsAre(Pair(Eq(TutorialId::kHoldingSpaceTourPrototype1), _),
                  Pair(Eq(TutorialId::kHoldingSpaceTourPrototype2), _)));
}

// HoldingSpaceTourControllerDragAndDropTest -----------------------------------

// Base class for drag-and-drop tests of the `HoldingSpaceTourController`,
// parameterized by (a) whether to drag Files app data and (b) whether to
// complete the drop (as opposed to cancelling it).
class HoldingSpaceTourControllerDragAndDropTest
    : public HoldingSpaceTourControllerTest,
      public testing::WithParamInterface<
          std::tuple</*drag_files_app_data=*/bool, /*complete_drop=*/bool>> {
 public:
  // Whether to drag Files app data given test parameterization.
  bool drag_files_app_data() const { return std::get<0>(GetParam()); }

  // Whether to complete the drop (as opposed to cancelling it) given test
  // parameterization.
  bool complete_drop() const { return std::get<1>(GetParam()); }

  // Moves the mouse to the center of the specified `widget`.
  void MoveMouseTo(views::Widget* widget) {
    GetEventGenerator()->MoveMouseTo(
        widget->GetWindowBoundsInScreen().CenterPoint(), /*count=*/10);
  }

  // Moves the mouse by the specified `x` and `y` offsets.
  void MoveMouseBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        event_generator->current_screen_location() + gfx::Vector2d(x, y),
        /*count=*/10);
  }

  // Presses and releases the key associated with the specified `key_code`.
  void PressAndReleaseKey(ui::KeyboardCode key_code) {
    GetEventGenerator()->PressAndReleaseKey(key_code);
  }

  // Presses/releases the left mouse button.
  void PressLeftButton() { GetEventGenerator()->PressLeftButton(); }
  void ReleaseLeftButton() { GetEventGenerator()->ReleaseLeftButton(); }

  // Sets a duration multiplier for animations.
  void SetAnimationDurationMultiplier(float duration_multiplier) {
    scoped_animation_duration_scale_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            duration_multiplier);
  }

  // Runs the message loop until the cached `help_bubble_` has closed. If no
  // `help_bubble_` is cached, this method returns immediately.
  void WaitForHelpBubbleClose() {
    if (!help_bubble_) {
      return;
    }
    base::RunLoop run_loop;
    base::CallbackListSubscription help_bubble_close_subscription =
        help_bubble_->AddOnCloseCallback(base::BindLambdaForTesting(
            [&](user_education::HelpBubble* help_bubble) { run_loop.Quit(); }));
    run_loop.Run();
  }

 private:
  // HoldingSpaceTourControllerTest:
  void SetUp() override {
    HoldingSpaceTourControllerTest::SetUp();

    // Prevent blocking during drag-and-drop sequences.
    ShellTestApi().drag_drop_controller()->SetDisableNestedLoopForTesting(true);

    // Mock `UserEducationDelegate::CreateHelpBubble()`.
    ON_CALL(*user_education_delegate(), CreateHelpBubble)
        .WillByDefault(
            Invoke([&](const AccountId& account_id, HelpBubbleId help_bubble_id,
                       user_education::HelpBubbleParams help_bubble_params,
                       ui::ElementIdentifier element_id,
                       ui::ElementContext element_context) {
              // Set `help_bubble_id` in extended properties.
              help_bubble_params.extended_properties.values().Merge(std::move(
                  user_education_util::CreateExtendedProperties(help_bubble_id)
                      .values()));

              // Attempt to create the `help_bubble`.
              auto help_bubble = help_bubble_factory_.CreateBubble(
                  ui::ElementTracker::GetElementTracker()
                      ->GetFirstMatchingElement(element_id, element_context),
                  std::move(help_bubble_params));

              // Cache the `help_bubble`, if one was created, and subscribe to
              // be notified when it closes in order to reset the cache.
              help_bubble_ = help_bubble.get();
              help_bubble_close_subscription_ =
                  help_bubble_
                      ? help_bubble_->AddOnCloseCallback(
                            base::BindLambdaForTesting(
                                [&](user_education::HelpBubble* help_bubble) {
                                  if (help_bubble == help_bubble_) {
                                    help_bubble_ = nullptr;
                                    help_bubble_close_subscription_ =
                                        base::CallbackListSubscription();
                                  }
                                }))
                      : base::CallbackListSubscription();

              // NOTE: May be `nullptr`.
              return help_bubble;
            }));
  }

  // Used to mock help bubble creation given that user education services in
  // the browser are non-existent for unit tests in Ash.
  user_education::test::TestHelpBubbleDelegate help_bubble_delegate_;
  HelpBubbleFactoryViewsAsh help_bubble_factory_{&help_bubble_delegate_};

  // The last help bubble created by `UserEducationDelegate::CreateHelpBubble()`
  // which is still open. Will be `nullptr` if no help bubble is currently open.
  raw_ptr<user_education::HelpBubble> help_bubble_ = nullptr;
  base::CallbackListSubscription help_bubble_close_subscription_;

  // Used to scale animation durations.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_scale_mode_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTourControllerDragAndDropTest,
    testing::Combine(/*drag_files_app_data=*/testing::Bool(),
                     /*complete_drop=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that the `HoldingSpaceTourController` handles drag-and-drop events
// as expected.
TEST_P(HoldingSpaceTourControllerDragAndDropTest, DragAndDrop) {
  // The holding space tray is always visible in the shelf when the
  // predictability feature is enabled. Force disable it so that we verify that
  // holding space visibility is updated by the `HoldingSpaceTourController`.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kHoldingSpacePredictability);

  // Set up a primary and secondary display and cache IDs.
  UpdateDisplay("1024x768,1024x768");
  const int64_t primary_display_id = GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateUserLogin(account_id);

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to crack file system URLs. Note that this is only
  // expected to occur when Files app data is dragged over the wallpaper.
  if (drag_files_app_data()) {
    EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
        .WillRepeatedly(Invoke([](const GURL& file_system_url) {
          return base::FilePath(base::StrCat(
              {"//path/to/", std::string(&file_system_url.spec().back())}));
        }));
  }

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      Shell::Get()->session_controller()->GetLastActiveUserPrefService());

  // Create and show a widget on the primary display from which data can be
  // drag-and-dropped.
  auto primary_widget = CreateTestWidgetForDisplayId(primary_display_id);
  primary_widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        if (drag_files_app_data()) {
          SetFilesAppData(data, u"file-system:a\nfile-system:b");
        }
      })));
  primary_widget->CenterWindow(gfx::Size(100, 100));
  primary_widget->Show();

  // Create and show a widget on the secondary display.
  auto secondary_widget = CreateTestWidgetForDisplayId(secondary_display_id);
  secondary_widget->CenterWindow(gfx::Size(100, 100));
  secondary_widget->Show();

  // Cache both shelves and holding space trays.
  auto* const primary_shelf = GetShelfForDisplayId(primary_display_id);
  auto* const secondary_shelf = GetShelfForDisplayId(secondary_display_id);
  auto* const primary_tray = GetHoldingSpaceTrayForShelf(primary_shelf);
  auto* const secondary_tray = GetHoldingSpaceTrayForShelf(secondary_shelf);

  // Set auto-hide behavior and verify that neither shelf is visible.
  primary_shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  secondary_shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(primary_shelf->IsVisible());
  EXPECT_FALSE(secondary_shelf->IsVisible());

  // Verify that neither holding space tray is visible.
  EXPECT_FALSE(primary_tray->GetVisible());
  EXPECT_FALSE(secondary_tray->GetVisible());

  // Ensure a non-zero animation duration so there is sufficient time to detect
  // pings before they are automatically destroyed on animation completion.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag data from the `primary_widget` to the wallpaper.
  MoveMouseTo(primary_widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/primary_widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  // Expect the holding space tray on the primary display to have a help bubble
  // and a ping if and only if Files app data was dragged. The holding space
  // tray on the secondary display should have neither.
  EXPECT_EQ(HasHelpBubble(primary_tray), drag_files_app_data());
  EXPECT_EQ(HasPing(primary_tray), drag_files_app_data());
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  // Expect the primary shelf and both holding space trays to be visible if
  // and only if Files app data was dragged.
  EXPECT_EQ(primary_shelf->IsVisible(), drag_files_app_data());
  EXPECT_EQ(primary_tray->GetVisible(), drag_files_app_data());
  EXPECT_EQ(secondary_tray->GetVisible(), drag_files_app_data());
  EXPECT_FALSE(secondary_shelf->IsVisible());

  // Expect the wallpaper on the primary display to be highlighted if and only
  // if Files app data was dragged. The wallpaper on the secondary display
  // should not be highlighted.
  EXPECT_EQ(HasWallpaperHighlight(primary_display_id), drag_files_app_data());
  EXPECT_FALSE(HasWallpaperHighlight(secondary_display_id));

  // Drag the data to a position just outside the `secondary_widget` so that the
  // cursor is over the wallpaper on the secondary display.
  MoveMouseTo(secondary_widget.get());
  MoveMouseBy(/*x=*/secondary_widget->GetWindowBoundsInScreen().width(),
              /*y=*/0);

  // Expect the holding space tray on the primary display to have a help bubble
  // and a ping if and only if Files app data was dragged. The holding space
  // tray on the secondary display should have neither.
  EXPECT_EQ(HasHelpBubble(primary_tray), drag_files_app_data());
  EXPECT_EQ(HasPing(primary_tray), drag_files_app_data());
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  // Expect the secondary shelf and both holding space trays to be visible if
  // and only if Files app data was dragged. The primary shelf should be visible
  // iff the holding space tray on the primary display has a help bubble.
  EXPECT_EQ(secondary_shelf->IsVisible(), drag_files_app_data());
  EXPECT_EQ(secondary_tray->GetVisible(), drag_files_app_data());
  EXPECT_EQ(primary_tray->GetVisible(), drag_files_app_data());
  EXPECT_EQ(primary_shelf->IsVisible(), HasHelpBubble(primary_tray));

  // Expect the wallpaper on the secondary display to be highlighted if and only
  // if Files app data was dragged. The wallpaper on the primary display should
  // not be highlighted.
  EXPECT_EQ(HasWallpaperHighlight(secondary_display_id), drag_files_app_data());
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));

  // Conditionally cancel the drop depending on test parameterization.
  if (!complete_drop()) {
    PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  const bool complete_drop_of_files_app_data =
      drag_files_app_data() && complete_drop();

  // If test parameterization dictates that Files app data will be dropped,
  // expect the holding space client to be instructed to pin files to the
  // holding space model.
  if (complete_drop_of_files_app_data) {
    EXPECT_CALL(holding_space_client,
                PinFiles(ElementsAre(Eq(base::FilePath("//path/to/a")),
                                     Eq(base::FilePath("//path/to/b")))))
        .WillOnce(
            Invoke([&](const std::vector<base::FilePath>& unpinned_file_paths) {
              holding_space_model.AddItems(CreateHoldingSpaceItems(
                  HoldingSpaceItem::Type::kPinnedFile, unpinned_file_paths));
            }));
  }

  // Release the left button. Note that this will complete the drop if it
  // wasn't already cancelled due to test parameterization.
  ReleaseLeftButton();
  FlushMessageLoop();

  // Expect the holding space tray on the primary display to have a help bubble
  // and a ping if and only if Files app data was dragged. The holding space
  // tray on the secondary display should have neither.
  EXPECT_EQ(HasHelpBubble(primary_tray), drag_files_app_data());
  EXPECT_EQ(HasPing(primary_tray), drag_files_app_data());
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  // Expect the primary shelf to be visible if and only if the holding space
  // tray on the primary display has a help bubble. The secondary shelf and both
  // holding space trays should be visible if and only if either:
  // (a) the holding space tray on the primary display has a help bubble, or
  // (b) Files app data was dropped.
  EXPECT_EQ(primary_shelf->IsVisible(), HasHelpBubble(primary_tray));
  EXPECT_EQ(secondary_shelf->IsVisible(), complete_drop_of_files_app_data);
  EXPECT_THAT(primary_tray->GetVisible(),
              AnyOf(Eq(HasHelpBubble(primary_tray)),
                    Eq(complete_drop_of_files_app_data)));
  EXPECT_THAT(secondary_tray->GetVisible(),
              AnyOf(Eq(HasHelpBubble(primary_tray)),
                    Eq(complete_drop_of_files_app_data)));

  // Expect no wallpaper to be highlighted.
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));
  EXPECT_FALSE(HasWallpaperHighlight(secondary_display_id));

  // Wait for the help bubble to close, if one exists. Note that animation
  // durations are first scaled to zero to prevent having to wait for shelf/
  // tray animations to complete before checking state.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  WaitForHelpBubbleClose();
  FlushMessageLoop();

  // Expect no help bubbles or pings.
  EXPECT_FALSE(HasHelpBubble(primary_tray));
  EXPECT_FALSE(HasPing(primary_tray));
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  // Expect the primary shelf to no longer be visible, but the secondary shelf
  // and both holding space trays should be visible if and only if Files app
  // data was dropped.
  EXPECT_FALSE(primary_shelf->IsVisible());
  EXPECT_EQ(secondary_shelf->IsVisible(), complete_drop_of_files_app_data);
  EXPECT_EQ(primary_tray->GetVisible(), complete_drop_of_files_app_data);
  EXPECT_EQ(secondary_tray->GetVisible(), complete_drop_of_files_app_data);

  // Expect no wallpaper to be highlighted.
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));
  EXPECT_FALSE(HasWallpaperHighlight(secondary_display_id));

  // If Files app data was dropped, the holding space bubble should be visible
  // on the secondary display.
  if (complete_drop_of_files_app_data) {
    EXPECT_TRUE(secondary_tray->GetBubbleWidget()->IsVisible());
    secondary_tray->GetBubbleWidget()->CloseNow();
  }

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

}  // namespace ash
