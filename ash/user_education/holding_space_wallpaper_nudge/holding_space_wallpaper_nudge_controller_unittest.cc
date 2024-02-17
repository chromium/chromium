// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_controller.h"

#include <array>
#include <iterator>
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
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
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
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"
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
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using ::ash::holding_space_metrics::EventSource;
using ::ash::holding_space_metrics::ItemAction;
using ::ash::holding_space_metrics::PodAction;
using ::ash::holding_space_wallpaper_nudge_metrics::IneligibleReason;
using ::ash::holding_space_wallpaper_nudge_metrics::Interaction;
using ::ash::holding_space_wallpaper_nudge_metrics::SuppressedReason;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::BucketsAreArray;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Conditional;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Le;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArgs;

// Helpers ---------------------------------------------------------------------

HoldingSpaceTray* GetHoldingSpaceTrayForShelf(Shelf* shelf) {
  return shelf->GetStatusAreaWidget()->holding_space_tray();
}

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
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
      type,
      HoldingSpaceFile(
          file_path, HoldingSpaceFile::FileSystemType::kTest,
          GURL(base::StrCat({"file-system:", file_path.BaseName().value()}))),
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
  std::optional<HelpBubbleId> help_bubble_id =
      UserEducationHelpBubbleController::Get()->GetHelpBubbleId(
          kHoldingSpaceTrayElementId,
          views::ElementTrackerViews::GetContextForView(tray));

  // Add failures if the help bubble is not the one that's expected.
  EXPECT_EQ(help_bubble_id.value_or(HelpBubbleId::kHoldingSpaceWallpaperNudge),
            HelpBubbleId::kHoldingSpaceWallpaperNudge);

  return help_bubble_id.has_value();
}

bool HasPing(HoldingSpaceTray* tray) {
  std::optional<PingId> ping_id =
      UserEducationPingController::Get()->GetPingId(tray);

  // Add failures if the ping is not the one that's expected.
  EXPECT_EQ(ping_id.value_or(PingId::kHoldingSpaceWallpaperNudge),
            PingId::kHoldingSpaceWallpaperNudge);

  return ping_id.has_value();
}

bool HasPinnedFilesPlaceholder(TrayBubbleView* bubble_view) {
  return bubble_view->GetViewByID(
      kHoldingSpacePinnedFilesSectionPlaceholderLabelId);
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
        HoldingSpaceWallpaperNudgeController::kHighlightLayerName) {
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

// LayerDestructionWaiter ------------------------------------------------------

// A class that waits for a layer to be destroyed.
class LayerDestructionWaiter : public ui::LayerObserver {
 public:
  void Wait(ui::Layer* layer) {
    observation_.Observe(layer);
    run_loop_.Run();
  }

 private:
  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override {
    observation_.Reset();
    run_loop_.Quit();
  }

  base::ScopedObservation<ui::Layer, ui::LayerObserver> observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace

// HoldingSpaceWallpaperNudgeControllerTest ------------------------------------

// Base class for tests of the `HoldingSpaceWallpaperNudgeController`.
class HoldingSpaceWallpaperNudgeControllerTestBase
    : public UserEducationAshTestBase {
 public:
  HoldingSpaceWallpaperNudgeControllerTestBase(
      std::optional<bool> auto_open_enabled,
      std::optional<bool> counterfactual_enabled,
      std::optional<bool> drop_to_pin_enabled,
      bool force_eligibility_enabled,
      base::test::TaskEnvironment::TimeSource time_source)
      : UserEducationAshTestBase(time_source) {
    // NOTE: The `HoldingSpaceWallpaperNudgeController` exists only when the
    // Holding Space wallpaper nudge feature is enabled. Controller existence is
    // verified in test coverage for the controller's owner.
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    base::FieldTrialParams params;

    if (auto_open_enabled.has_value()) {
      params.emplace("auto-open", auto_open_enabled.value() ? "true" : "false");
    }

    if (counterfactual_enabled.has_value()) {
      params.emplace("is-counterfactual",
                     counterfactual_enabled.value() ? "true" : "false");
    }

    if (drop_to_pin_enabled.has_value()) {
      params.emplace("drop-to-pin",
                     drop_to_pin_enabled.value() ? "true" : "false");
    }

    enabled.emplace_back(features::kHoldingSpaceWallpaperNudge, params);

    if (force_eligibility_enabled) {
      enabled.emplace_back(
          features::kHoldingSpaceWallpaperNudgeForceEligibility,
          base::FieldTrialParams());
    } else {
      disabled.emplace_back(
          features::kHoldingSpaceWallpaperNudgeForceEligibility);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  // Moves the mouse to the center of the specified `view`.
  void MoveMouseTo(const views::View* view) {
    GetEventGenerator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint(),
                                     /*count=*/10);
  }

  // Moves the mouse to the center of the specified `widget`.
  void MoveMouseTo(const views::Widget* widget) {
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

  // Waits until the ping on `tray` has closed. If the given `tray` has no ping,
  // this method returns immediately.
  void WaitForPingFinish(HoldingSpaceTray* tray) {
    if (!HasPing(tray)) {
      return;
    }

    for (auto* layer : tray->GetLayersInOrder()) {
      if (layer->name() == UserEducationPingController::kPingParentLayerName) {
        LayerDestructionWaiter().Wait(layer);
        return;
      }
    }
  }

 protected:
  // UserEducationAshTestBase:
  void SetUp() override {
    UserEducationAshTestBase::SetUp();

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

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

// HoldingSpaceWallpaperNudgeControllerTest ------------------------------------

// Base class for tests that verify general Holding Space wallpaper nudge
// behavior.
class HoldingSpaceWallpaperNudgeControllerTest
    : public HoldingSpaceWallpaperNudgeControllerTestBase {
 public:
  HoldingSpaceWallpaperNudgeControllerTest()
      : HoldingSpaceWallpaperNudgeControllerTestBase(
            /*auto_open_enabled=*/true,
            /*counterfactual_enabled=*/false,
            /*drop_to_pin_enabled=*/false,
            /*force_eligibility_enabled=*/false,
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}

 private:
  // HoldingSpaceWallpaperNudgeControllerTestBase:
  void SetUp() override {
    HoldingSpaceWallpaperNudgeControllerTestBase::SetUp();

    // Provide an implementation of `IsNewUser()` which always returns true,
    // since this test only deals with one session.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(
            testing::ReturnRefOfCopy(std::make_optional<bool>(true)));
  }
};

TEST_F(HoldingSpaceWallpaperNudgeControllerTest,
       HideBubbleAndTrayOnHoldingSpaceEmptied) {
  // The holding space tray is always visible in the shelf when the
  // predictability feature is enabled. Force disable it so that we verify that
  // holding space visibility is updated by the
  // `HoldingSpaceWallpaperNudgeController`.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kHoldingSpacePredictability);

  // Set animation durations to zero.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to pin files to holding space when so instructed.
  EXPECT_CALL(holding_space_client, PinFiles)
      .WillOnce(WithArgs<0>(
          Invoke([&](const std::vector<base::FilePath>& unpinned_file_paths) {
            holding_space_model.AddItems(CreateHoldingSpaceItems(
                HoldingSpaceItem::Type::kPinnedFile, unpinned_file_paths));
          })));

  // Configure the client to crack file system URLs.
  EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
      .WillRepeatedly(Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

  // Create and show a `widget` from which data can be drag-and-dropped.
  const int64_t display_id = GetPrimaryDisplay().id();
  auto widget = CreateTestWidgetForDisplayId(display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  // Cache the `shelf` and holding space `tray`.
  auto* const shelf = GetShelfForDisplayId(display_id);
  auto* const tray = GetHoldingSpaceTrayForShelf(shelf);

  // Drag data from the `widget` to the wallpaper to show the nudge. Expect a
  // help bubble to be anchored to the holding space `tray`.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);
  FlushMessageLoop();
  EXPECT_TRUE(HasHelpBubble(tray));

  // Drop the data on the holding space `tray`. Expect the help bubble to still
  // be anchored to the `tray`.
  MoveMouseTo(tray);
  ReleaseLeftButton();
  FlushMessageLoop();
  EXPECT_TRUE(HasHelpBubble(tray));

  // Open holding space. Expect the holding space `tray` and bubble to be
  // visible, but do not expect a help bubble since it would overlap with
  // holding space.
  tray->ShowBubble();
  EXPECT_TRUE(tray->GetVisible());
  EXPECT_TRUE(tray->GetBubbleWidget()->IsVisible());
  EXPECT_FALSE(HasHelpBubble(tray));

  // Dropping the data on the `tray` will have resulted in files being pinned to
  // holding space. Simulate the user un-pinning those files.
  EXPECT_FALSE(holding_space_model.items().empty());
  holding_space_model.RemoveAll();
  FlushMessageLoop();

  // Verify that the holding space `tray` and bubble are no longer visible.
  EXPECT_FALSE(tray->GetVisible());
  EXPECT_FALSE(tray->GetBubbleWidget());

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

TEST_F(HoldingSpaceWallpaperNudgeControllerTest, HideBubbleOnHoldingSpaceOpen) {
  // The holding space tray is always visible in the shelf when the
  // predictability feature is enabled. Force disable it so that we verify that
  // holding space visibility is updated by the
  // `HoldingSpaceWallpaperNudgeController`.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kHoldingSpacePredictability);

  // Set up a primary and secondary display and cache IDs.
  UpdateDisplay("1024x768,1024x768");
  const int64_t primary_display_id = GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to crack file system URLs. Note that this is only
  // expected to occur when Files app data is dragged over the wallpaper.
  EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
      .WillRepeatedly(Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  // Needed by the client to create the placeholder.
  EXPECT_CALL(holding_space_client, IsDriveDisabled)
      .WillRepeatedly(testing::Return(false));

  // Create and show a widget on the primary display from which data can be
  // drag-and-dropped.
  auto widget = CreateTestWidgetForDisplayId(primary_display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  // Set animation durations to zero to speed things up.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

  // Cache both shelves and holding space trays.
  auto* const primary_shelf = GetShelfForDisplayId(primary_display_id);
  auto* const secondary_shelf = GetShelfForDisplayId(secondary_display_id);
  auto* const primary_tray = GetHoldingSpaceTrayForShelf(primary_shelf);
  auto* const secondary_tray = GetHoldingSpaceTrayForShelf(secondary_shelf);

  // Drag data from the `widget` to the wallpaper to show the nudge, then
  // cancel the drag immediately.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  ReleaseLeftButton();

  // Expect only the primary display's holding space tray to have a help bubble.
  EXPECT_TRUE(HasHelpBubble(primary_tray));
  EXPECT_FALSE(HasHelpBubble(secondary_tray));

  // Expect the state not to change at all if the secondary display's holding
  // space bubble is opened, as it does not overlap with the help bubble.
  secondary_tray->ShowBubble();
  EXPECT_TRUE(HasHelpBubble(primary_tray));
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  secondary_tray->CloseBubble();

  // Expect the help bubble to close if the primary display's holding space is
  // opened, as that would overlap.
  primary_tray->ShowBubble();
  EXPECT_FALSE(HasHelpBubble(primary_tray));
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  primary_tray->CloseBubble();

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// HoldingSpaceWallpaperNudgeControllerDragAndDropTest -------------------------

// Base class for drag-and-drop tests of the
// `HoldingSpaceWallpaperNudgeController`, parameterized by (a) whether the
// drop-to-pin param is available and enabled, (b) whether to drag Files app
// data, and (c) whether to complete the drop (as opposed to cancelling it).
class HoldingSpaceWallpaperNudgeControllerDragAndDropTest
    : public HoldingSpaceWallpaperNudgeControllerTestBase,
      public testing::WithParamInterface<
          std::tuple</*auto_open_enabled=*/std::optional<bool>,
                     /*drop_to_pin_enabled=*/std::optional<bool>,
                     /*drag_files_app_data=*/bool,
                     /*complete_drop=*/bool>> {
 public:
  HoldingSpaceWallpaperNudgeControllerDragAndDropTest()
      : HoldingSpaceWallpaperNudgeControllerTestBase(
            auto_open_enabled(),
            /*counterfactual_enabled=*/false,
            drop_to_pin_enabled(),
            /*force_eligibility_enabled=*/false,
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}

  // Whether the auto-open feature param is enabled given test parameterization.
  std::optional<bool> auto_open_enabled() const {
    return std::get<0>(GetParam());
  }

  // Whether the drop-to-pin feature param is enabled given test
  // parameterization.
  std::optional<bool> drop_to_pin_enabled() const {
    return std::get<1>(GetParam());
  }

  // Whether to drag Files app data given test parameterization.
  bool drag_files_app_data() const { return std::get<2>(GetParam()); }

  // Whether to complete the drop (as opposed to cancelling it) given test
  // parameterization.
  bool complete_drop() const { return std::get<3>(GetParam()); }

 private:
  // HoldingSpaceWallpaperNudgeControllerTestBase:
  void SetUp() override {
    HoldingSpaceWallpaperNudgeControllerTestBase::SetUp();

    // Provide an implementation of `IsNewUser()` which always returns true,
    // since this test only deals with one session.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(
            testing::ReturnRefOfCopy(std::make_optional<bool>(true)));
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceWallpaperNudgeControllerDragAndDropTest,
    testing::Combine(
        /*auto_open_enabled=*/testing::Values(std::nullopt, false, true),
        /*drop_to_pin_enabled=*/testing::Values(std::nullopt, false, true),
        /*drag_files_app_data=*/testing::Bool(),
        /*complete_drop=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that the `HoldingSpaceWallpaperNudgeController` handles
// drag-and-drop events as expected.
TEST_P(HoldingSpaceWallpaperNudgeControllerDragAndDropTest, DragAndDrop) {
  // The holding space tray is always visible in the shelf when the
  // predictability feature is enabled. Force disable it so that we verify that
  // holding space visibility is updated by the
  // `HoldingSpaceWallpaperNudgeController`.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kHoldingSpacePredictability);

  // Set up a primary and secondary display and cache IDs.
  UpdateDisplay("1024x768,1024x768");
  const int64_t primary_display_id = GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

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

  // Needed by the client to create the placeholder.
  EXPECT_CALL(holding_space_client, IsDriveDisabled)
      .WillRepeatedly(testing::Return(false));

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

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

  const bool data_is_droppable =
      drag_files_app_data() && drop_to_pin_enabled().value_or(false);

  // Expect the wallpaper on the primary display to be highlighted if and only
  // if Files app data was dragged and the drop-to-pin behavior is enabled. The
  // wallpaper on the secondary display should not be highlighted.
  EXPECT_EQ(HasWallpaperHighlight(primary_display_id), data_is_droppable);
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
  // if Files app data was dragged and drop-to-pin is enabled. The wallpaper on
  // the primary display should not be highlighted.
  EXPECT_EQ(HasWallpaperHighlight(secondary_display_id), data_is_droppable);
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));

  // Conditionally cancel the drop depending on test parameterization.
  if (!complete_drop()) {
    PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  const bool complete_drop_of_files_app_data =
      drag_files_app_data() && complete_drop();
  const bool accept_drop_of_files_app_data =
      complete_drop_of_files_app_data && drop_to_pin_enabled().value_or(false);

  // If test parameterization dictates that Files app data will be accepted,
  // expect the holding space client to be instructed to pin files to the
  // holding space model.
  if (accept_drop_of_files_app_data) {
    EXPECT_CALL(holding_space_client,
                PinFiles(ElementsAre(Eq(base::FilePath("//path/to/a")),
                                     Eq(base::FilePath("//path/to/b"))),
                         Eq(holding_space_metrics::EventSource::kWallpaper)))
        .WillOnce(WithArgs<0>(
            Invoke([&](const std::vector<base::FilePath>& unpinned_file_paths) {
              holding_space_model.AddItems(CreateHoldingSpaceItems(
                  HoldingSpaceItem::Type::kPinnedFile, unpinned_file_paths));
            })));
  }

  // Release the left button. Note that this will complete the drop if it
  // wasn't already cancelled due to test parameterization.
  ReleaseLeftButton();
  FlushMessageLoop();

  // Expect the holding space tray on the primary display to have a ping and a
  // help bubble if and only if Files app data was dragged. The holding space
  // tray on the secondary display should have neither a help bubble nor a ping.
  EXPECT_EQ(HasHelpBubble(primary_tray), drag_files_app_data());
  EXPECT_EQ(HasPing(primary_tray), drag_files_app_data());
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  // Expect the primary shelf to be visible if and only if the holding space
  // tray on the primary display has a help bubble. The secondary shelf should
  // be visible if and only if Files app data was accepted. Both holding space
  // trays should be visible if and only if either:
  // (a) the holding space tray on the primary display has a help bubble, or
  // (b) Files app data was accepted.
  EXPECT_EQ(primary_shelf->IsVisible(), HasHelpBubble(primary_tray));
  EXPECT_EQ(secondary_shelf->IsVisible(), accept_drop_of_files_app_data);
  EXPECT_THAT(primary_tray->GetVisible(),
              AnyOf(Eq(HasHelpBubble(primary_tray)),
                    Eq(accept_drop_of_files_app_data)));
  EXPECT_THAT(secondary_tray->GetVisible(),
              AnyOf(Eq(HasHelpBubble(primary_tray)),
                    Eq(accept_drop_of_files_app_data)));

  // Expect no wallpaper to be highlighted.
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));
  EXPECT_FALSE(HasWallpaperHighlight(secondary_display_id));

  // Wait for the ping to finish and the help bubble to close, if either exists.
  // Note that animation durations are first scaled to zero to prevent having to
  // wait for shelf/ tray animations to complete before checking state.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  WaitForPingFinish(primary_tray);
  WaitForHelpBubbleClose();
  FlushMessageLoop();

  // Expect no help bubbles or pings.
  EXPECT_FALSE(HasHelpBubble(primary_tray));
  EXPECT_FALSE(HasPing(primary_tray));
  EXPECT_FALSE(HasHelpBubble(secondary_tray));
  EXPECT_FALSE(HasPing(secondary_tray));

  const bool holding_space_should_be_auto_opened =
      accept_drop_of_files_app_data && auto_open_enabled().value_or(true);

  // Expect the primary shelf to no longer be visible, but the secondary shelf
  // should be visible only if holding space was auto-opened. Both holding space
  // trays should be visible if and only if Files app data was accepted.
  EXPECT_FALSE(primary_shelf->IsVisible());
  EXPECT_EQ(secondary_shelf->IsVisible(), holding_space_should_be_auto_opened);
  EXPECT_EQ(primary_tray->GetVisible(), accept_drop_of_files_app_data);
  EXPECT_EQ(secondary_tray->GetVisible(), accept_drop_of_files_app_data);

  // Expect no wallpaper to be highlighted.
  EXPECT_FALSE(HasWallpaperHighlight(primary_display_id));
  EXPECT_FALSE(HasWallpaperHighlight(secondary_display_id));

  // If holding space was auto-opened, the holding space bubble should be
  // visible on the secondary display.
  if (holding_space_should_be_auto_opened) {
    EXPECT_TRUE(secondary_tray->GetBubbleWidget()->IsVisible());
    secondary_tray->GetBubbleWidget()->CloseNow();
  }

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// HoldingSpaceWallpaperNudgeControllerEligibilityTest -------------------------

// Base class for tests that verify the Holding Space wallpaper nudge is shown
// only to users that meet the eligibility criteria.
class HoldingSpaceWallpaperNudgeControllerEligibilityTest
    : public HoldingSpaceWallpaperNudgeControllerTestBase,
      public testing::WithParamInterface<std::tuple<
          /*force_user_eligibility=*/bool,
          /*is_new_user_cross_device=*/std::optional<bool>,
          /*is_new_user_locally=*/bool,
          /*is_managed_user=*/bool,
          user_manager::UserType>> {
 public:
  HoldingSpaceWallpaperNudgeControllerEligibilityTest()
      : HoldingSpaceWallpaperNudgeControllerTestBase(
            /*auto_open_enabled=*/true,
            /*counterfactual_enabled=*/false,
            /*drop_to_pin_enabled=*/true,
            /*force_eligibility_enabled=*/ForceUserEligibility(),
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Returns whether user eligibility is forced based on test parameterization.
  bool ForceUserEligibility() const { return std::get<0>(GetParam()); }

  // Returns the reason for the user's ineligibility based on test
  // parameterization. Note that this does not consider forced user eligibility.
  std::optional<IneligibleReason> GetUserIneligibleReason() const {
    if (GetUserType() != user_manager::UserType::kRegular) {
      return IneligibleReason::kUserTypeNotRegular;
    }
    if (IsManagedUser()) {
      return IneligibleReason::kManagedAccount;
    }
    if (!IsNewUserFirstLoginLocally()) {
      return IneligibleReason::kUserNotNewLocally;
    }
    if (!IsNewUserFirstLoginCrossDevice()) {
      return IneligibleReason::kUserNewnessNotAvailable;
    }
    if (IsNewUserFirstLoginCrossDevice() == false) {
      return IneligibleReason::kUserNotNewCrossDevice;
    }
    return std::nullopt;
  }

  // Returns the user type based on test parameterization.
  user_manager::UserType GetUserType() const { return std::get<4>(GetParam()); }

  // Returns whether the user is managed based on test parameterization.
  bool IsManagedUser() const { return std::get<3>(GetParam()); }

  // Returns whether the user should be considered "new" cross-device based on
  // test parameterization and if the user has previously completed a session.
  std::optional<bool> IsNewUserFirstLoginCrossDevice() const {
    return is_users_first_session_ && std::get<1>(GetParam());
  }

  // Returns whether the user should be considered "new" locally based on test
  // parameterization and if the user has previously completed a session.
  bool IsNewUserFirstLoginLocally() const {
    return is_users_first_session_ && std::get<2>(GetParam());
  }

  // Marks that the user has completed a session, so that they are no longer
  // considered "new" according to the session.
  void MarkUserFirstSessionComplete() { is_users_first_session_ = false; }

 private:
  // HoldingSpaceWallpaperNudgeControllerTestBase:
  void SetUp() override {
    HoldingSpaceWallpaperNudgeControllerTestBase::SetUp();

    // Provide an implementation of `IsNewUser()` which returns whether a given
    // user should be considered "new" cross-device based on test
    // parameterization.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(
            testing::ReturnRefOfCopy(IsNewUserFirstLoginCrossDevice()));
  }

  // Tracks whether the user being tested has logged in before.
  bool is_users_first_session_ = true;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceWallpaperNudgeControllerEligibilityTest,
    testing::Combine(
        /*force_user_eligibility=*/::testing::Bool(),
        /*is_new_user_cross_device=*/
        ::testing::Values(std::make_optional(true),
                          std::make_optional(false),
                          std::nullopt),
        /*is_new_user_locally=*/::testing::Bool(),
        /*is_managed_user=*/::testing::Bool(),
        ::testing::Values(user_manager::UserType::kArcKioskApp,
                          user_manager::UserType::kChild,
                          user_manager::UserType::kGuest,
                          user_manager::UserType::kKioskApp,
                          user_manager::UserType::kPublicAccount,
                          user_manager::UserType::kRegular,
                          user_manager::UserType::kWebKioskApp)));

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceWallpaperNudgeControllerEligibilityTest,
       FirstSessionMarked) {
  const bool expect_first_session_marked =
      !GetUserIneligibleReason().has_value();

  const auto before = base::Time::Now();

  // Log in a user type based on parameterization.
  auto* session = GetSessionControllerClient();
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  session->AddUserSession(account_id.GetUserEmail(), GetUserType(),
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/IsNewUserFirstLoginLocally(),
                          /*given_name=*/std::string(), IsManagedUser());
  session->SwitchActiveUser(account_id);
  session->SetSessionState(session_manager::SessionState::ACTIVE);

  const auto after = base::Time::Now();

  const std::optional<base::Time> first_session_time =
      holding_space_wallpaper_nudge_prefs::GetTimeOfFirstEligibleSession(
          GetLastActiveUserPrefService());

  EXPECT_THAT(first_session_time,
              Conditional(expect_first_session_marked,
                          AllOf(Ge(before), Le(after)), Eq(std::nullopt)));
}

// Verifies that the Holding Space wallpaper nudge is shown only for new, non-
// managed users, and will still be shown as appropriate after a user logs out
// and back in.
TEST_P(HoldingSpaceWallpaperNudgeControllerEligibilityTest, UserEligibility) {
  const auto ineligible_reason = GetUserIneligibleReason();
  const bool expect_eligibility = ForceUserEligibility() || !ineligible_reason;

  base::HistogramTester histogram_tester;

  // Log in a user type based on parameterization.
  auto* session = GetSessionControllerClient();
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  session->AddUserSession(account_id.GetUserEmail(), GetUserType(),
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/IsNewUserFirstLoginLocally(),
                          /*given_name=*/std::string(), IsManagedUser());
  session->SwitchActiveUser(account_id);
  session->SetSessionState(session_manager::SessionState::ACTIVE);

  // Verify user eligibility histograms are recorded after first login.
  // NOTE: The `IneligibleReason::kMinValue` fallback value below is never used,
  // it's only necessary because the `Conditional()` is not lazily evaluated.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Ash.HoldingSpaceWallpaperNudge.Eligible"),
      BucketsAre(Bucket(/*sample=*/!ineligible_reason, /*count=*/1u)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Ash.HoldingSpaceWallpaperNudge.IneligibleReason"),
              Conditional(ineligible_reason.has_value(),
                          BucketsAre(Bucket(
                              /*sample=*/ineligible_reason.value_or(
                                  IneligibleReason::kMinValue),
                              /*count=*/1u)),
                          IsEmpty()));

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  if (expect_eligibility) {
    // Configure the client to pin files to holding space when so instructed.
    EXPECT_CALL(holding_space_client, PinFiles)
        .WillRepeatedly(WithArgs<0>(
            Invoke([&](const std::vector<base::FilePath>& unpinned_file_paths) {
              holding_space_model.AddItems(CreateHoldingSpaceItems(
                  HoldingSpaceItem::Type::kPinnedFile, unpinned_file_paths));
            })));

    // Configure the client to crack file system URLs.
    EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
        .WillRepeatedly(Invoke([](const GURL& file_system_url) {
          return base::FilePath(base::StrCat(
              {"//path/to/", std::string(&file_system_url.spec().back())}));
        }));
  }

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

  const int64_t display_id = GetPrimaryDisplay().id();

  // Create and show a widget from which data can be drag-and-dropped.
  auto widget = CreateTestWidgetForDisplayId(display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  auto* const tray =
      GetHoldingSpaceTrayForShelf(GetShelfForDisplayId(display_id));

  // Ensure a non-zero animation duration so there is sufficient time to
  // detect pings before they are automatically destroyed on animation
  // completion.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag data from the `widget` to the wallpaper.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  // Expect the holding space tray to have a help bubble and a ping, and the
  // wallpaper to have a highlight depending on expected eligibility.
  EXPECT_EQ(HasHelpBubble(tray), expect_eligibility);
  EXPECT_EQ(HasPing(tray), expect_eligibility);
  EXPECT_EQ(HasWallpaperHighlight(display_id), expect_eligibility);

  // Drop data on the wallpaper and verify that it results in files being added
  // to holding space if and only if the user is eligible.
  ReleaseLeftButton();
  FlushMessageLoop();
  EXPECT_NE(holding_space_model.items().empty(), expect_eligibility);

  // Holding space should be auto-opened depending on expected eligibility.
  EXPECT_THAT(
      tray->GetBubbleWidget(),
      Conditional(expect_eligibility,
                  Property(&views::Widget::IsVisible, IsTrue()), IsNull()));

  // Remove all items from holding space, if any exist, so that any drag-and-
  // dropped files that were added can be re-pinned in the next session.
  holding_space_model.RemoveAll();
  FlushMessageLoop();

  // Reset state for the next session, using scaled animations to save time.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  task_environment()->AdvanceClock(
      user_education::kDefaultTimeoutWithoutButtons);
  WaitForPingFinish(tray);
  WaitForHelpBubbleClose();
  FlushMessageLoop();

  // Set checks for user "new-ness" to be `false` to confirm eligibility
  // persists across sessions. Note that forced eligibility is not persisted.
  MarkUserFirstSessionComplete();

  // Reset the session and log in again.
  session->Reset();
  session->AddUserSession(account_id.GetUserEmail(), GetUserType(),
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/false,
                          /*given_name=*/std::string(), IsManagedUser());
  session->SwitchActiveUser(account_id);
  session->SetSessionState(session_manager::SessionState::ACTIVE);

  // Verify that user eligibility histograms are not recorded on future logins.
  // NOTE: The `IneligibleReason::kMinValue` fallback value below is never used,
  // it's only necessary because the `Conditional()` is not lazily evaluated.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Ash.HoldingSpaceWallpaperNudge.Eligible"),
      BucketsAre(Bucket(/*sample=*/!ineligible_reason, /*count=*/1u)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Ash.HoldingSpaceWallpaperNudge.IneligibleReason"),
              Conditional(ineligible_reason.has_value(),
                          BucketsAre(Bucket(
                              /*sample=*/ineligible_reason.value_or(
                                  IneligibleReason::kMinValue),
                              /*count=*/1u)),
                          IsEmpty()));

  // Advance the clock because nudges can only be shown once per day.
  task_environment()->AdvanceClock(base::Hours(24));

  // Ensure a non-zero animation duration so there is sufficient time to
  // detect pings before they are automatically destroyed on animation
  // completion.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag data from the `widget` to the wallpaper.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  //  Expect the holding space tray to have a help bubble and a ping, and the
  //  wallpaper to have a highlight depending on expected eligibility.
  EXPECT_EQ(HasHelpBubble(tray), expect_eligibility);
  EXPECT_EQ(HasPing(tray), expect_eligibility);
  EXPECT_EQ(HasWallpaperHighlight(display_id), expect_eligibility);

  // Drop data on the wallpaper and verify that it results in files being added
  // to holding space if and only if the user is eligible.
  ReleaseLeftButton();
  FlushMessageLoop();
  EXPECT_NE(holding_space_model.items().empty(), expect_eligibility);

  // Holding space should be auto-opened depending on expected eligibility.
  EXPECT_THAT(
      tray->GetBubbleWidget(),
      Conditional(expect_eligibility,
                  Property(&views::Widget::IsVisible, IsTrue()), IsNull()));

  // TODO(http://b/325676397): Remove after fixing destruction order issues.
  // Close holding space, if it was showing.
  tray->CloseBubble();
  FlushMessageLoop();

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// HoldingSpaceWallpaperNudgeControllerRateLimitingTest ------------------------

// Base class for tests that verify the Holding Space wallpaper nudge is
// properly rate limited to avoid spamming the user.
// TODO(http://b/274659315): Add coverage for the reduced rate limiting
// parameter of the forced eligibility feature flag.
class HoldingSpaceWallpaperNudgeControllerRateLimitingTest
    : public HoldingSpaceWallpaperNudgeControllerTestBase,
      public testing::WithParamInterface<
          /*drop_to_pin_enabled=*/std::optional<bool>> {
 public:
  HoldingSpaceWallpaperNudgeControllerRateLimitingTest()
      : HoldingSpaceWallpaperNudgeControllerTestBase(
            /*auto_open_enabled=*/true,
            /*counterfactual_enabled=*/false,
            drop_to_pin_enabled(),
            /*force_eligibility_enabled=*/false,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Whether the drop-to-pin feature param is enabled.
  std::optional<bool> drop_to_pin_enabled() const { return GetParam(); }

  // HoldingSpaceWallpaperNudgeControllerTestBase:
  void SetUp() override {
    HoldingSpaceWallpaperNudgeControllerTestBase::SetUp();

    // Provide an implementation of `IsNewUser()` that always returns true. This
    // test suite is not concerned with user new-ness.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(
            testing::ReturnRefOfCopy(std::make_optional<bool>(true)));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceWallpaperNudgeControllerRateLimitingTest,
                         testing::Values(std::nullopt, false, true));

// Tests -----------------------------------------------------------------------

// Verifies that the Holding Space wallpaper nudge is only shown once per day,
// and a maximum total of three times.
TEST_P(HoldingSpaceWallpaperNudgeControllerRateLimitingTest, RateLimiting) {
  const int64_t display_id = GetPrimaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to crack file system URLs.
  EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
      .WillRepeatedly(Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  // Needed by the client to create the placeholder.
  EXPECT_CALL(holding_space_client, IsDriveDisabled)
      .WillRepeatedly(testing::Return(false));

  // Create and show a widget from which data can be drag-and-dropped.
  auto widget = CreateTestWidgetForDisplayId(display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  auto* const shelf = GetShelfForDisplayId(display_id);
  auto* const tray = GetHoldingSpaceTrayForShelf(shelf);

  // Autohide the shelf so that the shelf visibility behavior can be verified.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(shelf->IsVisible());
  EXPECT_FALSE(tray->GetVisible());

  const bool drop_to_pin_behavior_expected =
      drop_to_pin_enabled().value_or(false);

  size_t nudge_shown_count = 0u;

  for (size_t day = 0; day < 3; ++day) {
    base::HistogramTester histogram_tester;

    // Ensure a non-zero animation duration so there is sufficient time to
    // detect pings before they are automatically destroyed on animation
    // completion.
    SetAnimationDurationMultiplier(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // Drag data from the `widget` to the wallpaper.
    MoveMouseTo(widget.get());
    PressLeftButton();
    MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

    // Expect the holding space tray to have a help bubble and a ping.
    EXPECT_TRUE(HasHelpBubble(tray));
    EXPECT_TRUE(HasPing(tray));
    ++nudge_shown_count;

    // The shelf and holding space tray should show if the nudge is showing.
    EXPECT_TRUE(shelf->IsVisible());
    EXPECT_TRUE(tray->GetVisible());

    // The wallpaper highlight should also show if drop-to-pin is enabled.
    EXPECT_EQ(HasWallpaperHighlight(display_id), drop_to_pin_behavior_expected);

    // Verify that no nudge suppression related metrics were recorded.
    histogram_tester.ExpectTotalCount(
        "Ash.HoldingSpaceWallpaperNudge.SuppressedReason", /*count=*/0);

    // Reset the UI state, using zero-scaled animations to save time.
    SetAnimationDurationMultiplier(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    PressAndReleaseKey(ui::VKEY_ESCAPE);
    ReleaseLeftButton();
    task_environment()->AdvanceClock(
        user_education::kDefaultTimeoutWithoutButtons);
    WaitForPingFinish(tray);
    WaitForHelpBubbleClose();
    FlushMessageLoop();

    // Drag data again, now that the nudge has already been shown recently.
    SetAnimationDurationMultiplier(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    MoveMouseTo(widget.get());
    PressLeftButton();
    MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

    // Now the nudge should not be shown, as it was shown in the last 24 hours.
    EXPECT_FALSE(HasHelpBubble(tray));
    EXPECT_FALSE(HasPing(tray));

    // The shelf and tray should be visible if the user is dragging and
    // drop-to-pin is disabled to allow them to drop onto holding space. They
    // both should be hidden if drop-to-pin is enabled because we want to
    // encourage that behavior instead.
    EXPECT_NE(shelf->IsVisible(), drop_to_pin_behavior_expected);
    EXPECT_NE(tray->GetVisible(), drop_to_pin_behavior_expected);

    // Even if not showing the nudge, the wallpaper highlight should be shown if
    // drop-to-pin is enabled.
    EXPECT_EQ(HasWallpaperHighlight(display_id), drop_to_pin_behavior_expected);

    // Verify that nudge suppression related metrics were recorded.
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Ash.HoldingSpaceWallpaperNudge.SuppressedReason"),
        Conditional(nudge_shown_count >= 3,
                    BucketsAre(Bucket(SuppressedReason::kCountLimitReached, 1)),
                    BucketsAre(Bucket(SuppressedReason::kTimePeriod, 1))));

    // Reset the UI state, using zero-scaled animations to save time.
    SetAnimationDurationMultiplier(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    PressAndReleaseKey(ui::VKEY_ESCAPE);
    ReleaseLeftButton();
    FlushMessageLoop();

    // Every 24 hours, it should be possible for the nudge to show again once.
    task_environment()->AdvanceClock(base::Hours(24));
  }

  base::HistogramTester histogram_tester;

  // After the 3rd time, the nudge should not show again even after 24 hours.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  EXPECT_FALSE(HasHelpBubble(tray));
  EXPECT_FALSE(HasPing(tray));
  EXPECT_NE(shelf->IsVisible(), drop_to_pin_behavior_expected);
  EXPECT_NE(tray->GetVisible(), drop_to_pin_behavior_expected);
  EXPECT_EQ(HasWallpaperHighlight(display_id),
            drop_to_pin_enabled().value_or(false));

  // Verify that nudge suppression related metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      "Ash.HoldingSpaceWallpaperNudge.SuppressedReason",
      /*sample=*/SuppressedReason::kCountLimitReached, /*count=*/1);

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// Verifies that the Holding Space wallpaper nudge is not shown for users that
// have pinned a file before.
TEST_P(HoldingSpaceWallpaperNudgeControllerRateLimitingTest,
       UserHasPinnedFiles) {
  const int64_t display_id = GetPrimaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to crack file system URLs.
  EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
      .WillRepeatedly(Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  // Create and show a widget from which data can be drag-and-dropped.
  auto widget = CreateTestWidgetForDisplayId(display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  auto* const shelf = GetShelfForDisplayId(display_id);
  auto* const tray = GetHoldingSpaceTrayForShelf(shelf);

  // Modify prefs directly to indicate that the user has pinned a file. Note
  // that it doesn't matter if a file is currently pinned, only that they have
  // used the functionality at some point.
  holding_space_prefs::MarkTimeOfFirstPin(GetLastActiveUserPrefService());

  // Make animations non-zero so that the checks below don't miss them.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  base::HistogramTester histogram_tester;

  // Drag a file over the wallpaper, and expect that the nudge does not show.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  EXPECT_FALSE(HasHelpBubble(tray));
  EXPECT_FALSE(HasPing(tray));

  // The wallpaper highlight for drop-to-pin functionality should still appear
  // when the nudge is suppressed.
  EXPECT_EQ(HasWallpaperHighlight(display_id),
            drop_to_pin_enabled().value_or(false));

  // Verify that nudge suppression related metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      "Ash.HoldingSpaceWallpaperNudge.SuppressedReason",
      /*sample=*/SuppressedReason::kUserHasPinned, /*count=*/1);

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// HoldingSpaceWallpaperNudgeControllerCounterfactualTest ----------------------

// Base class for tests of the `HoldingSpaceWallpaperNudgeController` which are
// concerned with the behavior of counterfactual experiment arms.
class HoldingSpaceWallpaperNudgeControllerCounterfactualTest
    : public HoldingSpaceWallpaperNudgeControllerTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*counterfactual_enabled=*/std::optional<bool>,
                     /*drop_to_pin_enabled=*/std::optional<bool>>> {
 public:
  HoldingSpaceWallpaperNudgeControllerCounterfactualTest()
      : HoldingSpaceWallpaperNudgeControllerTestBase(
            /*auto_open_enabled=*/true,
            counterfactual_enabled(),
            drop_to_pin_enabled(),
            /*force_eligibility_enabled=*/false,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Whether the is-counterfactual feature parameter is enabled.
  std::optional<bool> counterfactual_enabled() const {
    return std::get<1>(GetParam());
  }

  // Whether the drop-to-pin feature parameter is enabled.
  std::optional<bool> drop_to_pin_enabled() const {
    return std::get<0>(GetParam());
  }

 private:
  // HoldingSpaceWallpaperNudgeControllerTestBase:
  void SetUp() override {
    HoldingSpaceWallpaperNudgeControllerTestBase::SetUp();

    // Provide an implementation of `IsNewUser()` that always returns `true`.
    // This test suite is not concerned with user new-ness.
    ON_CALL(*user_education_delegate(), IsNewUser)
        .WillByDefault(
            testing::ReturnRefOfCopy(std::make_optional<bool>(true)));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceWallpaperNudgeControllerCounterfactualTest,
                         testing::Combine(
                             /*counterfactual_enabled=*/
                             ::testing::Values(std::make_optional(true),
                                               std::make_optional(false),
                                               std::nullopt),
                             /*drop_to_pin_enabled=*/
                             ::testing::Values(std::make_optional(true),
                                               std::make_optional(false),
                                               std::nullopt)));

// Tests -----------------------------------------------------------------------

// Verifies that the holding space wallpaper nudge is prevented from showing if
// enabled counterfactually as part of an experiment arm.
TEST_P(HoldingSpaceWallpaperNudgeControllerCounterfactualTest,
       PreventsHoldingSpaceWallpaperNudgeCounterfactualArms) {
  base::HistogramTester histogram_tester;

  const int64_t display_id = GetPrimaryDisplay().id();
  const bool expect_counterfactual = counterfactual_enabled().value_or(false);
  const bool expect_drop_to_pin =
      !expect_counterfactual && drop_to_pin_enabled().value_or(false);

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Configure the client to crack file system URLs.
  EXPECT_CALL(holding_space_client, CrackFileSystemUrl)
      .WillRepeatedly(Invoke([](const GURL& file_system_url) {
        return base::FilePath(base::StrCat(
            {"//path/to/", std::string(&file_system_url.spec().back())}));
      }));

  if (expect_drop_to_pin) {
    // Needed by the client to create the placeholder.
    EXPECT_CALL(holding_space_client, IsDriveDisabled)
        .WillRepeatedly(testing::Return(false));
  }

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

  if (expect_drop_to_pin) {
    EXPECT_CALL(holding_space_client,
                PinFiles(ElementsAre(Eq(base::FilePath("//path/to/a")),
                                     Eq(base::FilePath("//path/to/b"))),
                         Eq(holding_space_metrics::EventSource::kWallpaper)))
        .WillOnce(WithArgs<0>(
            Invoke([&](const std::vector<base::FilePath>& unpinned_file_paths) {
              holding_space_model.AddItems(CreateHoldingSpaceItems(
                  HoldingSpaceItem::Type::kPinnedFile, unpinned_file_paths));
            })));
  }

  // Create and show a widget from which data can be drag-and-dropped.
  auto widget = CreateTestWidgetForDisplayId(display_id);
  widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        SetFilesAppData(data, u"file-system:a\nfile-system:b");
      })));
  widget->CenterWindow(gfx::Size(100, 100));
  widget->Show();

  auto* const shelf = GetShelfForDisplayId(display_id);
  auto* const tray = GetHoldingSpaceTrayForShelf(shelf);

  // Autohide the shelf so that the shelf visibility behavior can be verified.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(shelf->IsVisible());
  EXPECT_FALSE(tray->GetVisible());

  // Ensure a non-zero animation duration so there is sufficient time to
  // detect pings before they are automatically destroyed on animation
  // completion.
  SetAnimationDurationMultiplier(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Verify initial state of nudge shown related metrics.
  constexpr char kShownMetric[] = "Ash.HoldingSpaceWallpaperNudge.Shown";
  constexpr char kDurationMetric[] = "Ash.HoldingSpaceWallpaperNudge.Duration";
  histogram_tester.ExpectTotalCount(kShownMetric, /*count=*/0);
  histogram_tester.ExpectTotalCount(kDurationMetric, /*count=*/0);

  // Drag data from the `widget` to the wallpaper.
  MoveMouseTo(widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  // Verify an interaction metric was recorded.
  constexpr char kInteractionMetric[] =
      "Ash.HoldingSpaceWallpaperNudge.Interaction.Count";
  histogram_tester.ExpectUniqueSample(
      kInteractionMetric, /*sample=*/Interaction::kDraggedFileOverWallpaper,
      /*count=*/1);

  // Verify the nudge shown metric was recorded. Note that this metric is
  // recorded even if the experiment is enabled counterfactually.
  histogram_tester.ExpectUniqueSample(kShownMetric, /*sample=*/1, /*count=*/1);

  // Expect the holding space tray to have a help bubble and a ping only iff
  // the experiment is enabled non-counterfactually.
  EXPECT_NE(HasHelpBubble(tray), expect_counterfactual);
  EXPECT_NE(HasPing(tray), expect_counterfactual);

  // The shelf and holding space tray should show iff the experiment is enabled
  // non-counterfactually.
  EXPECT_NE(shelf->IsVisible(), expect_counterfactual);
  EXPECT_NE(tray->GetVisible(), expect_counterfactual);

  // The wallpaper highlight should show if drop-to-pin behavior is enabled.
  EXPECT_EQ(HasWallpaperHighlight(display_id), expect_drop_to_pin);

  {
    // Mock elapsed timers so that they are deterministic for testing.
    base::ScopedMockElapsedTimersForTest scoped_elapsed_timers;

    // Release the left button. This will complete the drop and pin items to the
    // holding space if the drop-to-pin behavior is enabled.
    ReleaseLeftButton();
    FlushMessageLoop();

    // Verify an interaction metric was recorded.
    histogram_tester.ExpectBucketCount(
        kInteractionMetric, /*sample=*/Interaction::kDroppedFileOnWallpaper,
        /*count=*/1);

    // Wait for the help bubble to close, if one exists, and verify that the
    // nudge duration metric was recorded. Note that this metric is recorded
    // even if the experiment is enabled counterfactually.
    WaitForHelpBubbleClose();
    histogram_tester.ExpectUniqueTimeSample(
        kDurationMetric,
        /*sample=*/
        expect_counterfactual
            ? base::TimeDelta()
            : base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
        /*count=*/1);
  }

  // Expect the dropped items to be pinned to holding space iff drop-to-pin
  // behavior is enabled.
  size_t expected_items = expect_drop_to_pin ? 2u : 0u;
  EXPECT_EQ(holding_space_model.items().size(), expected_items);

  // Expect the tray bubble to be shown after successful drop-to-pin behavior.
  if (expect_drop_to_pin) {
    EXPECT_TRUE(tray->GetBubbleWidget()->IsVisible());
    tray->GetBubbleWidget()->CloseNow();
  }

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

// HoldingSpaceWallpaperNudgeMetricsTest ---------------------------------------

// Base class for tests of the `HoldingSpaceWallpaperNudgeController` which are
// concerned with metrics, parameterized by whether the holding space wallpaper
// nudge is enabled.
class HoldingSpaceWallpaperNudgeMetricsTest
    : public UserEducationAshTestBase,
      public ::testing::WithParamInterface</*nudge_enabled=*/bool> {
 public:
  HoldingSpaceWallpaperNudgeMetricsTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpaceWallpaperNudge,
        IsHoldingSpaceWallpaperNudgeEnabled());
  }

  // Whether the holding space wallpaper nudge is enabled given test
  // parameterization.
  bool IsHoldingSpaceWallpaperNudgeEnabled() const { return GetParam(); }

 private:
  // UserEducationAshTestBase:
  void SetUp() override {
    UserEducationAshTestBase::SetUp();

    // When the holding space wallpaper nudge is enabled, provide an
    // implementation of `IsNewUser()` which always returns `true`, thereby
    // indicating that the user is new cross-device and therefore eligible for
    // the experiment.
    if (IsHoldingSpaceWallpaperNudgeEnabled()) {
      ON_CALL(*user_education_delegate(), IsNewUser)
          .WillByDefault(ReturnRefOfCopy(std::make_optional<bool>(true)));
    }

    // Log in a regular user.
    const AccountId& account_id = AccountId::FromUserEmail("user@test");
    SimulateNewUserFirstLogin(account_id.GetUserEmail());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceWallpaperNudgeMetricsTest,
                         /*nudge_enabled=*/testing::Bool());

// Tests -----------------------------------------------------------------------

// Verifies that when the user pins their first file to holding space the
// appropriate histograms are recorded.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordsFirstPin) {
  base::HistogramTester histogram_tester;

  PrefService* const prefs = GetLastActiveUserPrefService();

  // Simulate the holding space wallpaper nudge having been shown if, and only
  // if, the feature is enabled.
  constexpr size_t kShowNudgeCount = 2u;
  if (IsHoldingSpaceWallpaperNudgeEnabled()) {
    for (size_t i = 0u; i < kShowNudgeCount; ++i) {
      holding_space_wallpaper_nudge_prefs::MarkNudgeShown(prefs);
    }
  }

  // Simulate the user pinning their first file to holding space.
  holding_space_prefs::MarkTimeOfFirstPin(prefs);

  // Verify that the holding space wallpaper nudge metrics associated with the
  // user pinning their first file to holding space are recorded if, and only
  // if, the feature is enabled.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Ash.HoldingSpaceWallpaperNudge.ShownBeforeFirstPin"),
      Conditional(IsHoldingSpaceWallpaperNudgeEnabled(),
                  BucketsAre(Bucket(/*sample=*/kShowNudgeCount, /*count=*/1)),
                  IsEmpty()));
}

// Verifies that when item action histograms are recorded from the production
// holding space metrics code path, the corresponding interaction histograms
// from the wallpaper nudge experiment metrics code path are also recorded.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest,
       RecordsInteractionWhenRecordingItemAction) {
  struct TestCase {
    ItemAction item_action;
    EventSource event_source;
    std::vector<Interaction> expected_interactions;
  };

  // Set up `test_cases`.
  const std::array<TestCase, 15u> test_cases = {
      TestCase{
          .item_action = ItemAction::kCancel,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kCopy,
          .event_source = EventSource::kHoldingSpaceItemContextMenu,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kDrag,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kLaunch,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kPause,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kPinnedFileFromAnySource,
                                    Interaction::kPinnedFileFromPinButton},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kHoldingSpaceItemContextMenu,
          .expected_interactions = {Interaction::kPinnedFileFromAnySource,
                                    Interaction::kPinnedFileFromContextMenu},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kHoldingSpaceTray,
          .expected_interactions =
              {Interaction::kPinnedFileFromAnySource,
               Interaction::kPinnedFileFromHoldingSpaceDrop},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kFilesApp,
          .expected_interactions = {Interaction::kPinnedFileFromAnySource,
                                    Interaction::kPinnedFileFromFilesApp},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kTest,
          .expected_interactions = {Interaction::kPinnedFileFromAnySource},
      },
      TestCase{
          .item_action = ItemAction::kPin,
          .event_source = EventSource::kWallpaper,
          .expected_interactions = {Interaction::kPinnedFileFromAnySource,
                                    Interaction::kPinnedFileFromWallpaperDrop},
      },
      TestCase{
          .item_action = ItemAction::kRemove,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kResume,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kShowInFolder,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {Interaction::kUsedOtherItem,
                                    Interaction::kUsedPinnedItem},
      },
      TestCase{
          .item_action = ItemAction::kUnpin,
          .event_source = EventSource::kHoldingSpaceItem,
          .expected_interactions = {},
      },
  };

  // Create holding space `items`.
  const auto pinned_item = CreateHoldingSpaceItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("pinned"));
  const auto unpinned_item = CreateHoldingSpaceItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("unpinned"));
  const std::vector<const HoldingSpaceItem*> items = {pinned_item.get(),
                                                      unpinned_item.get()};

  // Partition `pinned_items` from `unpinned_items`.
  std::vector<const HoldingSpaceItem*> pinned_items;
  std::vector<const HoldingSpaceItem*> unpinned_items;
  base::ranges::partition_copy(
      items, std::back_inserter(pinned_items),
      std::back_inserter(unpinned_items),
      [](HoldingSpaceItem::Type type) {
        return type == HoldingSpaceItem::Type::kPinnedFile;
      },
      &HoldingSpaceItem::type);

  // Verify expectations for each `test_case`.
  for (const TestCase& test_case : test_cases) {
    base::HistogramTester histogram_tester;

    // Whenever an item action histogram is recorded from the production
    // holding space metrics code path...
    holding_space_metrics::RecordItemAction(items, test_case.item_action,
                                            test_case.event_source);

    // ...the corresponding interaction histogram from the wallpaper nudge
    // experiment metrics code path should also be recorded iff the nudge is
    // enabled.
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Ash.HoldingSpaceWallpaperNudge.Interaction.Count"),
                Conditional(IsHoldingSpaceWallpaperNudgeEnabled(),
                            BucketsAreArray(([&]() {
                              std::vector<Bucket> buckets;
                              for (Interaction interaction :
                                   test_case.expected_interactions) {
                                switch (interaction) {
                                  case Interaction::kUsedOtherItem:
                                    buckets.emplace_back(interaction,
                                                         unpinned_items.size());
                                    break;
                                  case Interaction::kUsedPinnedItem:
                                    buckets.emplace_back(interaction,
                                                         pinned_items.size());
                                    break;
                                  default:
                                    buckets.emplace_back(interaction,
                                                         items.size());
                                    break;
                                }
                              }
                              return buckets;
                            })()),
                            IsEmpty()));
  }
}

// Verifies that when pod action histograms are recorded from the production
// holding space metrics code path, the corresponding interaction histograms
// from the wallpaper nudge experiment metrics code path are also recorded.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest,
       RecordsInteractionWhenRecordingPodAction) {
  struct TestCase {
    PodAction pod_action;
    Interaction expected_interaction;
  };

  // Set up `kTestCases`.
  constexpr std::array<TestCase, 2u> kTestCases = {
      TestCase{.pod_action = PodAction::kDragAndDropToPin,
               .expected_interaction = Interaction::kDroppedFileOnHoldingSpace},
      TestCase{.pod_action = PodAction::kShowBubble,
               .expected_interaction = Interaction::kOpenedHoldingSpace},
  };

  // Verify expectations for each `test_case`.
  for (const TestCase& test_case : kTestCases) {
    base::HistogramTester histogram_tester;

    // Whenever a pod action histogram is recorded from the production
    // holding space metrics code path...
    holding_space_metrics::RecordPodAction(test_case.pod_action);

    // ...the corresponding interaction histogram from the wallpaper nudge
    // experiment metrics code path should also be recorded iff the nudge is
    // enabled.
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Ash.HoldingSpaceWallpaperNudge.Interaction.Count"),
        Conditional(IsHoldingSpaceWallpaperNudgeEnabled(),
                    BucketsAre(Bucket(test_case.expected_interaction, 1u)),
                    IsEmpty()));
  }
}

// HoldingSpaceWallpaperNudgePlaceholderTest -----------------------------------

// Base class for tests of the `HoldingSpaceWallpaperNudgeController` which are
// concerned with the placeholder shown in cases where the Holding Space is
// opened when empty.
class HoldingSpaceWallpaperNudgePlaceholderTest
    : public UserEducationAshTestBase,
      public testing::WithParamInterface</*nudge_enabled=*/bool> {
 public:
  HoldingSpaceWallpaperNudgePlaceholderTest() {
    // The Holding Space Wallpaper Nudge feature is parameterized, while the
    // Predictability and Suggestions experiments are explicitly disabled to
    // make sure we've isolated the placeholder's behavior as it pertains to the
    // nudge.
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features{
        features::kHoldingSpacePredictability,
        features::kHoldingSpaceSuggestions};
    (nudge_enabled() ? enabled_features : disabled_features)
        .push_back(features::kHoldingSpaceWallpaperNudge);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool nudge_enabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceWallpaperNudgePlaceholderTest,
                         /*nudge_enabled=*/testing::Bool());

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceWallpaperNudgePlaceholderTest, HasPinnedFilesPlaceholder) {
  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateNewUserFirstLogin(account_id.GetUserEmail());

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  testing::StrictMock<MockHoldingSpaceClient> holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Needed by the client to create the placeholder.
  EXPECT_CALL(holding_space_client, IsDriveDisabled)
      .WillRepeatedly(testing::Return(false));

  // Mark the holding space feature as available since there is no holding
  // space keyed service which would otherwise be responsible for doing so.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetLastActiveUserPrefService());

  auto* const tray = GetHoldingSpaceTrayForShelf(
      GetShelfForDisplayId(GetPrimaryDisplay().id()));

  tray->ShowBubble();
  EXPECT_EQ(HasPinnedFilesPlaceholder(tray->GetBubbleView()), nudge_enabled());
  tray->CloseBubble();

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

}  // namespace ash
