// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <array>
#include <deque>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/public/cpp/holding_space/mock_holding_space_controller_observer.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_tray_icon_preview.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/view_drawn_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_preview_view.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/drag_utils.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::RunUntil;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Property;

constexpr char kTestUser[] = "user@test";

// Helpers ---------------------------------------------------------------------

HoldingSpaceItem::InProgressCommand CreateInProgressCommand(
    HoldingSpaceCommandId command_id,
    int label_id,
    HoldingSpaceItem::InProgressCommand::Handler handler = base::DoNothing()) {
  return HoldingSpaceItem::InProgressCommand(
      command_id, label_id, &gfx::kNoneIcon, std::move(handler));
}

// A wrapper around `views::View::GetVisible()` with a null check for `view`.
bool IsViewVisible(const views::View* view) {
  return view && view->GetVisible();
}

void Click(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.ClickLeftButton();
}

void DoubleClick(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.DoubleClickLeftButton();
}

void RightClick(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.ClickRightButton();
}

void GestureScrollBy(const views::View* view, int offset_x, int offset_y) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  gfx::Point start(view->GetBoundsInScreen().CenterPoint()), end(start);
  end.Offset(offset_x, offset_y);
  ui::test::EventGenerator event_generator(root_window);
  event_generator.GestureScrollSequence(start, end,
                                        /*duration=*/base::Milliseconds(100),
                                        /*steps=*/10);
}

void GestureTap(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

ui::GestureEvent BuildGestureEvent(const gfx::Point& event_location,
                                   ui::EventType gesture_type) {
  return ui::GestureEvent(event_location.x(), event_location.y(), ui::EF_NONE,
                          ui::EventTimeForNow(),
                          ui::GestureEventDetails(gesture_type));
}

void LongPress(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  const gfx::Point& press_location = event_generator.current_screen_location();
  ui::GestureEvent long_press =
      BuildGestureEvent(press_location, ui::EventType::kGestureLongPress);
  event_generator.Dispatch(&long_press);

  ui::GestureEvent gesture_end =
      BuildGestureEvent(press_location, ui::EventType::kGestureEnd);
  event_generator.Dispatch(&gesture_end);
}

void MoveMouseTo(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint(), 10);
}

bool PressTabUntilFocused(const views::View* view, int max_count = 10) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  while (!view->HasFocus() && --max_count >= 0)
    event_generator.PressKey(ui::VKEY_TAB, ui::EF_NONE);
  return view->HasFocus();
}

std::unique_ptr<HoldingSpaceImage> CreateStubHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

std::vector<HoldingSpaceCommandId> GetHoldingSpaceCommandIds() {
  std::vector<HoldingSpaceCommandId> ids;
  for (int i = static_cast<int>(HoldingSpaceCommandId::kMinValue);
       i <= static_cast<int>(HoldingSpaceCommandId::kMaxValue); ++i)
    ids.push_back(static_cast<HoldingSpaceCommandId>(i));
  return ids;
}

size_t GetMaxVisibleItemCount(HoldingSpaceSectionId section_id) {
  return GetHoldingSpaceSection(section_id)->max_visible_item_count.value();
}

// Returns whether a context menu is currently showing.
bool IsShowingContextMenu() {
  return views::MenuController::GetActiveInstance();
}

// Returns the menu item matched by `id`.
const views::MenuItemView* GetMenuItemByCommandId(HoldingSpaceCommandId id) {
  if (!IsShowingContextMenu())
    return nullptr;
  if (auto* menu_item =
          views::MenuController::GetActiveInstance()->GetSelectedMenuItem()) {
    return menu_item->GetMenuItemByID(static_cast<int>(id));
  }
  return nullptr;
}

// ViewVisibilityChangedWaiter -------------------------------------------------

// A class capable of waiting until a view's visibility is changed.
class ViewVisibilityChangedWaiter : public views::ViewObserver {
 public:
  // Waits until the specified `view`'s visibility is changed.
  void Wait(views::View* view) {
    // Temporarily observe `view`.
    base::ScopedObservation<views::View, views::ViewObserver> observer{this};
    observer.Observe(view);

    // Loop until the `view`'s visibility is changed.
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();

    // Reset.
    wait_loop_.reset();
  }

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override {
    wait_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> wait_loop_;
};

// TransformRecordingLayerDelegate ---------------------------------------------

// A scoped `ui::LayerDelegate` which records information about transforms.
class ScopedTransformRecordingLayerDelegate : public ui::LayerDelegate {
 public:
  explicit ScopedTransformRecordingLayerDelegate(ui::Layer* layer)
      : layer_(layer), layer_delegate_(layer_->delegate()) {
    layer_->set_delegate(this);
    Reset();
  }

  ScopedTransformRecordingLayerDelegate(
      const ScopedTransformRecordingLayerDelegate&) = delete;
  ScopedTransformRecordingLayerDelegate& operator=(
      const ScopedTransformRecordingLayerDelegate&) = delete;

  ~ScopedTransformRecordingLayerDelegate() override {
    layer_->set_delegate(layer_delegate_);
  }

  // Resets recorded information.
  void Reset() {
    const gfx::Transform& transform = layer_->transform();
    did_animate_ = false;
    start_scale_ = end_scale_ = min_scale_ = max_scale_ = transform.To2dScale();
    start_translation_ = end_translation_ = min_translation_ =
        max_translation_ = transform.To2dTranslation();
  }

  // Returns true if an animation occurred.
  bool DidAnimate() const { return did_animate_; }

  // Returns true if a scale occurred.
  bool DidScale() const {
    return start_scale_ != min_scale_ || start_scale_ != max_scale_;
  }

  // Returns true if a translation occurred.
  bool DidTranslate() const {
    return start_translation_ != min_translation_ ||
           start_translation_ != max_translation_;
  }

  // Returns true if `layer_` scaled from `start` to `end`.
  bool ScaledFrom(const gfx::Vector2dF& start,
                  const gfx::Vector2dF& end) const {
    return start == start_scale_ && end == end_scale_;
  }

  // Returns true if `layer_` scaled within `min` and `max`.
  bool ScaledInRange(const gfx::Vector2dF& min,
                     const gfx::Vector2dF& max) const {
    return min == min_scale_ && max == max_scale_;
  }

  // Returns true if `layer_` translated from `start` to `end`.
  bool TranslatedFrom(const gfx::Vector2dF& start,
                      const gfx::Vector2dF& end) const {
    return start == start_translation_ && end == end_translation_;
  }

  // Returns true if `layer_` translated within `min` to `max`.
  bool TranslatedInRange(const gfx::Vector2dF& min,
                         const gfx::Vector2dF& max) const {
    return min == min_translation_ && max == max_translation_;
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_scale, float new_scale) override {}

  void OnLayerTransformed(const gfx::Transform& old_transform,
                          ui::PropertyChangeReason reason) override {
    const gfx::Transform& transform = layer_->transform();
    did_animate_ |= reason == ui::PropertyChangeReason::FROM_ANIMATION;
    end_scale_ = transform.To2dScale();
    end_translation_ = transform.To2dTranslation();
    min_scale_.SetToMin(end_scale_);
    max_scale_.SetToMax(end_scale_);
    min_translation_.SetToMin(end_translation_);
    max_translation_.SetToMax(end_translation_);
  }

  const raw_ptr<ui::Layer> layer_;
  const raw_ptr<ui::LayerDelegate> layer_delegate_;

  bool did_animate_ = false;
  gfx::Vector2dF start_scale_;
  gfx::Vector2dF start_translation_;
  gfx::Vector2dF end_scale_;
  gfx::Vector2dF end_translation_;
  gfx::Vector2dF min_scale_;
  gfx::Vector2dF max_scale_;
  gfx::Vector2dF min_translation_;
  gfx::Vector2dF max_translation_;
};

}  // namespace

// HoldingSpaceTrayTestBase ----------------------------------------------------

class HoldingSpaceTrayTestBase : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<HoldingSpaceTestApi>();
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());
    GetSessionControllerClient()->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  HoldingSpaceItem* AddItem(
      HoldingSpaceItem::Type type,
      const base::FilePath& path,
      const HoldingSpaceProgress& progress = HoldingSpaceProgress()) {
    return AddItemToModel(model(), type, path, progress);
  }

  HoldingSpaceItem* AddItemToModel(
      HoldingSpaceModel* target_model,
      HoldingSpaceItem::Type type,
      const base::FilePath& path,
      const HoldingSpaceProgress& progress = HoldingSpaceProgress()) {
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type,
            HoldingSpaceFile(
                path, HoldingSpaceFile::FileSystemType::kTest,
                GURL(base::StrCat({"filesystem:", path.BaseName().value()}))),
            progress, base::BindOnce(&CreateStubHoldingSpaceImage));
    HoldingSpaceItem* item_ptr = item.get();
    target_model->AddItem(std::move(item));
    return item_ptr;
  }

  HoldingSpaceItem* AddPartiallyInitializedItem(HoldingSpaceItem::Type type,
                                                const base::FilePath& path) {
    // Create a holding space item, and use it to create a serialized item
    // dictionary.
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type,
            HoldingSpaceFile(path, HoldingSpaceFile::FileSystemType::kTest,
                             GURL("filesystem:ignored")),
            base::BindOnce(&CreateStubHoldingSpaceImage));
    const base::Value::Dict serialized_holding_space_item = item->Serialize();
    std::unique_ptr<HoldingSpaceItem> deserialized_item =
        HoldingSpaceItem::Deserialize(
            serialized_holding_space_item,
            /*image_resolver=*/
            base::BindOnce(&CreateStubHoldingSpaceImage));

    HoldingSpaceItem* deserialized_item_ptr = deserialized_item.get();
    model()->AddItem(std::move(deserialized_item));
    return deserialized_item_ptr;
  }

  void RemoveAllItems() {
    model()->RemoveIf(
        base::BindRepeating([](const HoldingSpaceItem* item) { return true; }));
  }

  // The holding space tray is only visible in the shelf after the first holding
  // space item has been added. Most tests do not care about this so, as a
  // convenience, the time of first add will be marked prior to starting the
  // session when `pre_mark_time_of_first_add` is true.
  void StartSession(bool pre_mark_time_of_first_add = true) {
    if (pre_mark_time_of_first_add)
      MarkTimeOfFirstAdd();

    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void MarkTimeOfFirstAdd() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void MarkTimeOfFirstPin() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void SwitchToSecondaryUser(const std::string& user_id,
                             HoldingSpaceClient* client,
                             HoldingSpaceModel* model) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(user_account,
                                                                 client, model);
    GetSessionControllerClient()->AddUserSession(user_id);

    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));

    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void UnregisterModelForUser(const std::string& user_id) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, nullptr, nullptr);
  }

  void EnableTrayIconPreviews(std::string testUserEmail = kTestUser) {
    AccountId account_id = AccountId::FromUserEmail(testUserEmail);
    auto* prefs = GetSessionControllerClient()->GetUserPrefService(account_id);
    ASSERT_TRUE(prefs);
    holding_space_prefs::SetPreviewsEnabled(prefs, true);
  }

  HoldingSpaceTestApi* test_api() { return test_api_.get(); }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceModel* model() { return &holding_space_model_; }

  Shelf* GetShelf(const display::Display& display) {
    auto* const manager = Shell::Get()->window_tree_host_manager();
    auto* const window = manager->GetRootWindowForDisplayId(display.id());
    return Shelf::ForWindow(window);
  }

  HoldingSpaceTray* GetTray() {
    return GetTray(Shelf::ForWindow(Shell::GetRootWindowForNewWindows()));
  }

  HoldingSpaceTray* GetTray(Shelf* shelf) {
    return shelf->shelf_widget()->status_area_widget()->holding_space_tray();
  }

 private:
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
};

// TODO(crbug.com/1373911): Break up `HoldingSpaceTrayTest` so that tests are
// grouped by the component they're testing. Do not add new tests to this suite.
// If you have a test that truly belongs this file, use (or create) a test suite
// that inherits from `HoldingSpaceAshTestBase`.
class HoldingSpaceTrayTest : public HoldingSpaceTrayTestBase {
 public:
  HoldingSpaceTrayTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kHoldingSpaceSuggestions);
  }

  // Verifies that the user's preferences and the suggestion section's visual
  // appearance match a test's current scenario.
  void VerifySuggestionsSectionState(bool expanded, bool item_present) {
    AccountId account_id = AccountId::FromUserEmail(kTestUser);
    auto* prefs = GetSessionControllerClient()->GetUserPrefService(account_id);
    ASSERT_TRUE(prefs);

    const auto expected_chevron_model = ui::ImageModel::FromVectorIcon(
        expanded ? kChevronUpSmallIcon : kChevronDownSmallIcon,
        kColorAshIconColorSecondary, kHoldingSpaceSectionChevronIconSize);

    // Changes to the section's expanded state should be stored persistently.
    EXPECT_EQ(holding_space_prefs::IsSuggestionsExpanded(prefs), expanded);

    // The section header should be visible as long as suggestions are
    // available.
    views::View* const header = test_api()->GetSuggestionsSectionHeader();
    EXPECT_EQ(IsViewVisible(header), item_present);

    // The section header's accessibility data should indicate whether the
    // section is expanded or collapsed.
    ui::AXNodeData node_data;
    header->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    if (expanded) {
      EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
      EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
    } else {
      EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));
      EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
    }

    // The section header's chevron icon should indicate whether the section is
    // expanded or collapsed.
    auto* suggestions_section_chevron_icon =
        test_api()->GetSuggestionsSectionChevronIcon();
    EXPECT_TRUE(gfx::BitmapsAreEqual(
        *suggestions_section_chevron_icon->GetImage().bitmap(),
        *expected_chevron_model
             .Rasterize(suggestions_section_chevron_icon->GetColorProvider())
             .bitmap()));

    // The section content should be visible as long as suggestions are
    // available and the section is expanded.
    EXPECT_EQ(test_api()->GetSuggestionsSectionContainer()->GetVisible(),
              expanded && item_present);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Holding Space used to own the constant which determines its bubble's width
// but now shares a constant with the rest of the system UI bubbles. Holding
// Space UI is not yet implemented to be fully reactive to variable bubble
// widths, so this test adds a speed bump to (hopefully) prevent the shared
// constant from being updated and inadvertently breaking Holding Space UI.
TEST_F(HoldingSpaceTrayTest, BubbleHasExpectedWidth) {
  // Start session and verify the holding space tray is showing in the shelf.
  StartSession(/*pre_mark_time_of_first_add=*/true);
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the holding space bubble.
  test_api()->Show();
  EXPECT_TRUE(test_api()->IsShowing());

  // Verify holding space bubble width.
  views::View* const bubble = test_api()->GetBubble();
  ASSERT_TRUE(bubble);
  ViewDrawnWaiter().Wait(bubble);
  EXPECT_EQ(bubble->width(), 360);
}

TEST_F(HoldingSpaceTrayTest, ShowTrayButtonOnFirstUse) {
  StartSession(/*pre_mark_time_of_first_add=*/false);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  HoldingSpaceItem* item =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Show the bubble - both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // Remove the download item and verify the pinned files bubble, and the
  // tray button are still shown.
  model()->RemoveItem(item->id());
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  test_api()->Close();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  EXPECT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_FALSE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  test_api()->Show();

  // Add and remove a pinned item.
  HoldingSpaceItem* pinned_item =
      AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/pin"));
  MarkTimeOfFirstPin();
  model()->RemoveItem(pinned_item->id());

  // Verify that the pinned files bubble, and the tray button get hidden.
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  test_api()->Close();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_F(HoldingSpaceTrayTest, HideButtonWhenModelDetached) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/nullptr);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_FALSE(test_api()->IsShowingInShelf());
  UnregisterModelForUser("user@secondary");
}

TEST_F(HoldingSpaceTrayTest, HideButtonOnUserAddingScreen) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The tray button should be showing if the user has an item in holding space.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The tray button should be hidden if the user adding screen is running.
  SetUserAddingScreenRunning(true);
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The tray button should be showing if the user adding screen is finished.
  SetUserAddingScreenRunning(false);
  EXPECT_TRUE(test_api()->IsShowingInShelf());
}

TEST_F(HoldingSpaceTrayTest, AddingItemShowsTrayBubble) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_1->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a screen capture item - the button should be shown.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a pinned item - the button should be shown.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_3->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_F(HoldingSpaceTrayTest, TrayButtonNotShownForPartialItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add few partial items - the tray button should remain hidden.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kDownload,
                              base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_3"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_4"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Initialize one item, and verify the tray button gets shown.
  model()->InitializeOrRemoveItem(
      item_2->id(), HoldingSpaceFile(item_2->file().file_path,
                                     HoldingSpaceFile::FileSystemType::kTest,
                                     GURL("filesystem:fake_2")));

  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the initialized item - the shelf button should get hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Tests that a shelf config change just after an item has been removed does
// not cause a crash.
TEST_F(HoldingSpaceTrayTest, ShelfConfigChangeWithDelayedItemRemoval) {
  MarkTimeOfFirstPin();
  StartSession();

  // Create a test widget to force in-app shelf in tablet mode.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ASSERT_TRUE(widget);

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());

  model()->RemoveItem(item_1->id());
  TabletModeControllerTestApi().EnterTabletMode();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());

  model()->RemoveItem(item_2->id());
  TabletModeControllerTestApi().LeaveTabletMode();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Right clicking the holding space tray should show a context menu if the
// previews feature is enabled. Otherwise it should do nothing.
TEST_F(HoldingSpaceTrayTest, ShouldMaybeShowContextMenuOnRightClick) {
  StartSession();

  views::View* tray = test_api()->GetTray();
  ASSERT_TRUE(tray);

  EXPECT_FALSE(views::MenuController::GetActiveInstance());

  // Move the mouse to and perform a right click on `tray`.
  auto* root_window = tray->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(tray->GetBoundsInScreen().CenterPoint());
  event_generator.ClickRightButton();

  EXPECT_TRUE(views::MenuController::GetActiveInstance());
}

// Tests that a partially initialized screen recording item shows in the UI in
// the reverse order from added time rather than initialization time.
TEST_F(HoldingSpaceTrayTest,
       PartialScreenRecordingItemWithExistingScreenshotItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot items to fill up the section.
  HoldingSpaceItem* screenshot_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_1"));
  HoldingSpaceItem* screenshot_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_2"));
  HoldingSpaceItem* screenshot_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_3"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Initialize the screen recording item and verify it is not shown.
  model()->InitializeOrRemoveItem(
      screen_recording_item->id(),
      HoldingSpaceFile(screen_recording_item->file().file_path,
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:screen_recording")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the screen recording
  // item that was initialized late is shown.
  model()->RemoveItem(screenshot_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item_last =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording_last"));
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Initialize the screen recording item and verify it is shown first.
  model()->InitializeOrRemoveItem(
      screen_recording_item_last->id(),
      HoldingSpaceFile(screen_recording_item_last->file().file_path,
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:screen_recording")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_last->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Tests that partially initialized screenshot item shows in the UI in the
// reverse order from added time rather than initialization time.
TEST_F(HoldingSpaceTrayTest,
       PartialScreenshotItemWithExistingScreenRecordingItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screenshot item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* screenshot_item = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot recording items to fill up the section.
  HoldingSpaceItem* screen_recording_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* screen_recording_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* screen_recording_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Initialize the screenshot item and verify it is not shown.
  model()->InitializeOrRemoveItem(
      screenshot_item->id(),
      HoldingSpaceFile(screenshot_item->file().file_path,
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:fake_1")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is not shown.
  model()->RemoveItem(screen_recording_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Until the user has pinned an item, a placeholder should exist in the pinned
// files bubble which contains a chip to open the Files app.
TEST_F(HoldingSpaceTrayTest, PlaceholderContainsFilesAppChip) {
  StartSession(/*pre_mark_time_of_first_add=*/false);

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble. Both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // A chip to open the Files app should exist in the pinned files bubble.
  views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);
  views::View* files_app_chip =
      pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId);
  ASSERT_TRUE(files_app_chip);

  // Prior to being acted upon by the user, there should be no events logged to
  // the Files app chip histogram.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 0);

  // Click the chip and expect a call to open the Files app.
  EXPECT_CALL(*client(), OpenMyFiles);
  Click(files_app_chip);

  // After having been acted upon by the user, there should be a single click
  // event logged to the Files app chip histogram.
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 1);

  // Because the holding space model contains a download item, the holding space
  // tray should still be shown. The recent files bubble should be shown but
  // pinned files child bubble should have been hidden due to destruction of the
  // pinned files section placeholder which is no longer relevant.
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
}

// The pinned files section of holding space UI contains a placeholder if the
// user has never pinned a file. The placeholder contains a Files app chip to
// take the user to the Files app to pin their first file. Once the user has
// pressed the Files app chip, the pinned files section placeholder should be
// permanently hidden.
TEST_F(HoldingSpaceTrayTest, PlaceholderHiddenAfterFilesAppChipPressed) {
  StartSession(/*pre_mark_time_of_first_add=*/true);

  // The tray button should be shown because the user has previously added an
  // item to their holding space.
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble. Only the pinned files child bubble should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // A chip to open the Files app should exist in the pinned files bubble.
  views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);
  views::View* files_app_chip =
      pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId);
  ASSERT_TRUE(files_app_chip);

  // Click the chip and expect a call to open the Files app.
  EXPECT_CALL(*client(), OpenMyFiles);
  Click(files_app_chip);

  // Because the holding space is completely empty, clicking the Files app chip
  // should cause the holding space tray and all associated bubbles to hide.
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add a download item. This should cause the tray button to show.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show holding space UI. Because the Files app chip was previously pressed,
  // the recent files bubble should be shown but the pinned files bubble should
  // not.
  test_api()->Show();
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
}

// User should be able to expand and collapse the suggestions section by
// pressing the enter key on the suggestions section header.
TEST_F(HoldingSpaceTrayTest, EnterKeyTogglesSuggestionsExpanded) {
  StartSession();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("HoldingSpace.Suggestions.Action.All", 0);

  // Add a suggested item.
  AddItem(HoldingSpaceItem::Type::kLocalSuggestion,
          base::FilePath("/tmp/fake1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  ASSERT_EQ(test_api()->GetSuggestionChips().size(), 1u);

  // Focus the suggestions section header.
  auto* suggestions_section_header = test_api()->GetSuggestionsSectionHeader();
  ASSERT_TRUE(suggestions_section_header);

  // Verify that the section starts out expanded.
  {
    SCOPED_TRACE("Initially expanded.");
    VerifySuggestionsSectionState(/*expanded=*/true, /*item_present=*/true);
  }

  // Press ENTER and expect the section to collapse.
  EXPECT_TRUE(PressTabUntilFocused(suggestions_section_header));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kCollapse, 1);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kExpand, 0);
  {
    SCOPED_TRACE("First collapse.");
    VerifySuggestionsSectionState(/*expanded=*/false, /*item_present=*/true);
  }

  // Press ENTER and expect the section to expand.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kCollapse, 1);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kExpand, 1);
  {
    SCOPED_TRACE("First expand.");
    VerifySuggestionsSectionState(/*expanded=*/true, /*item_present=*/true);
  }

  // Remove the section's item and expect the section header to stop showing.
  RemoveAllItems();
  {
    SCOPED_TRACE("Item removed (expanded).");
    VerifySuggestionsSectionState(/*expanded=*/true, /*item_present=*/false);
  }

  // Add a suggested item and expect the section header to show again.
  AddItem(HoldingSpaceItem::Type::kLocalSuggestion,
          base::FilePath("/tmp/fake2"));
  {
    SCOPED_TRACE("Item added (expanded).");
    VerifySuggestionsSectionState(/*expanded=*/true, /*item_present=*/true);
  }

  // Verify that removing and adding an item works with the section collapsed.
  EXPECT_TRUE(PressTabUntilFocused(suggestions_section_header));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kCollapse, 2);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Suggestions.Action.All",
      holding_space_metrics::SuggestionsAction::kExpand, 1);
  {
    SCOPED_TRACE("Second collapse.");
    VerifySuggestionsSectionState(/*expanded=*/false, /*item_present=*/true);
  }

  RemoveAllItems();
  {
    SCOPED_TRACE("Item removed (collapsed).");
    VerifySuggestionsSectionState(/*expanded=*/false, /*item_present=*/false);
  }

  AddItem(HoldingSpaceItem::Type::kLocalSuggestion,
          base::FilePath("/tmp/fake3"));
  {
    SCOPED_TRACE("Item added (collapsed).");
    VerifySuggestionsSectionState(/*expanded=*/false, /*item_present=*/true);
  }
}

// User should be able to open the Downloads folder in the Files app by pressing
// the enter key on the Downloads section header.
TEST_F(HoldingSpaceTrayTest, EnterKeyOpensDownloads) {
  StartSession();

  // Add a download item.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Select the download item. Previously there was a bug where if a holding
  // space item view was selected, the enter key would *not* open Downloads.
  Click(download_chips[0]);
  EXPECT_TRUE(HoldingSpaceItemView::Cast(download_chips[0])->selected());

  // Focus the downloads section header.
  auto* downloads_section_header = test_api()->GetDownloadsSectionHeader();
  ASSERT_TRUE(downloads_section_header);
  EXPECT_TRUE(PressTabUntilFocused(downloads_section_header));

  // Press ENTER and expect an attempt to open the Downloads folder in the Files
  // app. There should be *no* attempts to open an holding space items.
  EXPECT_CALL(*client(), OpenItems).Times(0);
  EXPECT_CALL(*client(), OpenDownloads);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
}

// User should be able to launch selected holding space items by pressing the
// enter key.
TEST_F(HoldingSpaceTrayTest, EnterKeyOpensSelectedFiles) {
  StartSession();

  // Add three holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake3"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  const std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  const std::array<HoldingSpaceItemView*, 3> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(pinned_file_chips[1]),
      HoldingSpaceItemView::Cast(pinned_file_chips[2]),
  };

  // Press the enter key. The client should *not* attempt to open any items.
  EXPECT_CALL(*client(), OpenItems).Times(0);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Click an item. The view should be selected.
  Click(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Press the enter key. We expect the client to open the selected item.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceBubble),
                /*callback=*/_));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Shift-click on the second item. Both views should be selected.
  Click(item_views[1], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());

  // Press the enter key. We expect the client to open the selected items.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item(), item_views[1]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceBubble),
                /*callback=*/_));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Tab traverse to the last item.
  EXPECT_TRUE(PressTabUntilFocused(item_views[2]));

  // Press the enter key. The client should open only the focused item since
  // it was *not* selected prior to pressing the enter key.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[2]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
}

// Clicking on tote bubble background should deselect any selected items.
TEST_F(HoldingSpaceTrayTest, ClickBackgroundToDeselectItems) {
  StartSession();

  // Add two items.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  std::array<HoldingSpaceItemView*, 2> item_views = {
      HoldingSpaceItemView::Cast(download_chips[0]),
      HoldingSpaceItemView::Cast(download_chips[1])};

  // Click an item chip. The view should be selected.
  Click(download_chips[0]);
  ASSERT_TRUE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());

  // Clicking on the parent view should deselect item.
  Click(download_chips[0]->parent());
  ASSERT_FALSE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());

  // Click on both items to select them both.
  Click(download_chips[0], ui::EF_SHIFT_DOWN);
  Click(download_chips[1], ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(item_views[0]->selected());
  ASSERT_TRUE(item_views[1]->selected());

  // Clicking on the parent view should deselect both items.
  Click(download_chips[0]->parent());
  ASSERT_FALSE(item_views[0]->selected());
  ASSERT_FALSE(item_views[1]->selected());
}

// It should be possible to select multiple items in clamshell mode.
TEST_F(HoldingSpaceTrayTest, MultiselectInClamshellMode) {
  StartSession();

  // Add a few holding space items to populate each section.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake3"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake4"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake5"));

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  std::vector<views::View*> screen_capture_views =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 2u);
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);
  std::vector<HoldingSpaceItemView*> item_views(
      {HoldingSpaceItemView::Cast(pinned_file_chips[0]),
       HoldingSpaceItemView::Cast(screen_capture_views[0]),
       HoldingSpaceItemView::Cast(screen_capture_views[1]),
       HoldingSpaceItemView::Cast(download_chips[0]),
       HoldingSpaceItemView::Cast(download_chips[1])});

  // Shift-click the middle view. It should become selected.
  Click(item_views[2], ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Click the middle view. It should *not* become unselected.
  Click(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Shift-click the bottom view. We should now have selected a range.
  Click(item_views[4], ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.

  // Shift-click the top view. The previous range should be cleared and the
  // new range selected.
  Click(item_views[0], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_FALSE(item_views[4]->selected());

  // Control-click the bottom view. The previous range should still be selected
  // as well as the view that was just clicked.
  Click(item_views[4], ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_FALSE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.

  // Shift-click the second-from-the-bottom view. A new range should be selected
  // from the bottom view to the view that was just clicked. The previous range
  // that was selected should still be selected.
  Click(item_views[3], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[4]->selected());  // The previously clicked view.

  // Control-click the second-from-the-top view. The view that was just clicked
  // should now be unselected. No other views that were selected should change.
  Click(item_views[1], ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());

  // Add another holding space item. This should cause views for existing
  // holding space items to be destroyed and recreated.
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake6"));
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 3u);
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0]),
                HoldingSpaceItemView::Cast(screen_capture_views[0]),
                HoldingSpaceItemView::Cast(screen_capture_views[1]),
                HoldingSpaceItemView::Cast(screen_capture_views[2]),
                HoldingSpaceItemView::Cast(download_chips[0]),
                HoldingSpaceItemView::Cast(download_chips[1])};

  // Views for items previously selected should have selection restored.
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());  // The view for the new item.
  EXPECT_FALSE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());
  EXPECT_TRUE(item_views[5]->selected());

  // Shift-click the second-from-the-bottom view. A new range should be selected
  // from the previously clicked view to the view that was just clicked.
  Click(item_views[4], ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());  // The previously clicked view.
  EXPECT_TRUE(item_views[3]->selected());
  EXPECT_TRUE(item_views[4]->selected());  // The clicked view.
  EXPECT_TRUE(item_views[5]->selected());

  // Click the third-from-the-bottom view. Even though it was already selected
  // it should now be the only view selected.
  Click(item_views[3]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(item_views[3]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[4]->selected());
  EXPECT_FALSE(item_views[5]->selected());

  // Control-click the third-from-the-bottom view. There should no longer be
  // any views selected.
  Click(item_views[3], ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_FALSE(item_views[3]->selected());  // The clicked view.
  EXPECT_FALSE(item_views[4]->selected());
  EXPECT_FALSE(item_views[5]->selected());
}

// It should be possible to select multiple items in touch mode.
TEST_F(HoldingSpaceTrayTest, MultiselectInTouchMode) {
  StartSession();

  // Add a few holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake3"));

  // Show the bubble and cache holding space item views.
  test_api()->Show();
  const std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  std::array<HoldingSpaceItemView*, 3> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(pinned_file_chips[1]),
      HoldingSpaceItemView::Cast(pinned_file_chips[2])};

  // Long press an item. The view should be selected and a context menu shown.
  LongPress(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(views::MenuController::GetActiveInstance());

  // Close the context menu. The view that was long pressed should still be
  // selected.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Long press another item. Both views that were long pressed should be
  // selected and a context menu shown.
  LongPress(item_views[1]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());
  EXPECT_TRUE(views::MenuController::GetActiveInstance());

  // Close the context menu. Both views that were long pressed should still be
  // selected.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap one of the selected views. It should no longer be selected.
  GestureTap(item_views[0]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap one of the unselected views. It should become selected.
  GestureTap(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());
  EXPECT_TRUE(item_views[2]->selected());

  // Tap both selected views. No views should be selected.
  GestureTap(item_views[1]);
  GestureTap(item_views[2]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());
  EXPECT_FALSE(item_views[2]->selected());

  // Tap an unselected view. This is the only way to open an item via touch.
  // There must be *no* views currently selected when tapping a view.
  EXPECT_CALL(*client(), OpenItems)
      .WillOnce(
          testing::Invoke([&](const std::vector<const HoldingSpaceItem*>& items,
                              holding_space_metrics::EventSource event_source,
                              HoldingSpaceClient::SuccessCallback callback) {
            ASSERT_EQ(items.size(), 1u);
            EXPECT_EQ(items[0], item_views[2]->item());
            EXPECT_EQ(event_source,
                      holding_space_metrics::EventSource::kHoldingSpaceItem);
          }));
  GestureTap(item_views[2]);
  testing::Mock::VerifyAndClearExpectations(client());
}

// Verifies that selection UI is correctly represented depending on device state
// and the number of selected holding space item views.
TEST_F(HoldingSpaceTrayTest, SelectionUi) {
  StartSession();

  // Add both a chip-style and screen-capture-style holding space item.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake2"));

  // Show holding space UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Cache holding space item views.
  std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  std::vector<views::View*> screen_capture_views =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(screen_capture_views.size(), 1u);
  std::vector<HoldingSpaceItemView*> item_views = {
      HoldingSpaceItemView::Cast(pinned_file_chips[0]),
      HoldingSpaceItemView::Cast(screen_capture_views[0])};

  // Expects visibility of `view` to match `visible`.
  auto expect_visible = [](views::View* view, bool visible) {
    ASSERT_TRUE(view);
    EXPECT_EQ(view->GetVisible(), visible);
  };

  // Expects visibility of `item_view`'s checkmark to match `visible`.
  auto expect_checkmark_visible = [&](HoldingSpaceItemView* item_view,
                                      bool visible) {
    auto* checkmark = item_view->GetViewByID(kHoldingSpaceItemCheckmarkId);
    expect_visible(checkmark, visible);
  };

  // Expects visibility of `item_view`'s image to match `visible`.
  auto expect_image_visible = [&](HoldingSpaceItemView* item_view,
                                  bool visible) {
    auto* image = item_view->GetViewByID(kHoldingSpaceItemImageId);
    expect_visible(image, visible);
  };

  // Initially no holding space item views are selected.
  for (HoldingSpaceItemView* item_view : item_views) {
    EXPECT_FALSE(item_view->selected());
    expect_checkmark_visible(item_view, false);
    expect_image_visible(item_view, true);
  }

  // Select the first holding space item view.
  Click(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  // Since the device is not in tablet mode and only a single holding space item
  // view is selected, no checkmarks should be shown.
  for (HoldingSpaceItemView* item_view : item_views) {
    expect_checkmark_visible(item_view, false);
    expect_image_visible(item_view, true);
  }

  // Add the second holding space item view to the selection.
  Click(item_views[1], ui::EF_CONTROL_DOWN);

  // Because there are multiple holding space item views selected, checkmarks
  // should be shown. For chip-style holding space item views the checkmark
  // replaces the image.
  for (HoldingSpaceItemView* item_view : item_views) {
    EXPECT_TRUE(item_view->selected());
    expect_checkmark_visible(item_view, true);
    expect_image_visible(item_view, HoldingSpaceItem::IsScreenCaptureType(
                                        item_view->item()->type()));
  }

  // Remove the second holding space item. Note that its view was selected.
  HoldingSpaceController::Get()->model()->RemoveItem(item_views[1]->item_id());

  // Re-cache holding space item views as they will have been destroyed and
  // recreated when animating item view removal.
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  EXPECT_EQ(screen_capture_views.size(), 0u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0])};

  // The first (and only) holding space item view should still be selected
  // although it should no longer show its checkmark since now only a single
  // holding space item view is selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], false);
  expect_image_visible(item_views[0], true);

  // Switch to tablet mode. Note that this closes holding space UI.
  ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(test_api()->IsShowing());

  // Re-show holding space UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Cache holding space item views.
  pinned_file_chips = test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  screen_capture_views = test_api()->GetScreenCaptureViews();
  EXPECT_EQ(screen_capture_views.size(), 0u);
  item_views = {HoldingSpaceItemView::Cast(pinned_file_chips[0])};

  // Initially no holding space item views are selected.
  EXPECT_FALSE(item_views[0]->selected());

  // Select the first (and only) holding space item view.
  Click(item_views[0]);

  // In tablet mode, a selected holding space item view should always show its
  // checkmark even if it is the only holding space item view selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], true);
  expect_image_visible(item_views[0], false);

  // Switch out of tablet mode. Note that this *doesn't* close holding space UI.
  ShellTestApi().SetTabletModeEnabledForTest(false);
  ASSERT_TRUE(test_api()->IsShowing());

  // The first (and only) holding space item should still be selected but it
  // should update checkmark/image visibility given that it is the only holding
  // space item view selected.
  EXPECT_TRUE(item_views[0]->selected());
  expect_checkmark_visible(item_views[0], false);
  expect_image_visible(item_views[0], true);
}

// Verifies selection state after pressing primary/secondary actions.
TEST_F(HoldingSpaceTrayTest, SelectionWithPrimaryAndSecondaryActions) {
  StartSession();

  // Add multiple in-progress holding space items.
  std::vector<HoldingSpaceItem*> items = {
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"),
              HoldingSpaceProgress(0, 100)),
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake2"),
              HoldingSpaceProgress(0, 100))};

  // In-progress download items typically support in-progress commands.
  std::vector<const HoldingSpaceItem*> cancelled_items;
  std::vector<const HoldingSpaceItem*> paused_items;
  for (HoldingSpaceItem* item : items) {
    EXPECT_TRUE(item->SetInProgressCommands(
        {CreateInProgressCommand(
             HoldingSpaceCommandId::kCancelItem,
             IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_CANCEL,
             base::BindLambdaForTesting(
                 [&](const HoldingSpaceItem* item,
                     HoldingSpaceCommandId command_id,
                     holding_space_metrics::EventSource event_source) {
                   EXPECT_EQ(command_id, HoldingSpaceCommandId::kCancelItem);
                   EXPECT_EQ(
                       event_source,
                       holding_space_metrics::EventSource::kHoldingSpaceItem);
                   cancelled_items.push_back(item);
                 })),
         CreateInProgressCommand(
             HoldingSpaceCommandId::kPauseItem,
             IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PAUSE,
             base::BindLambdaForTesting(
                 [&](const HoldingSpaceItem* item,
                     HoldingSpaceCommandId command_id,
                     holding_space_metrics::EventSource event_source) {
                   EXPECT_EQ(command_id, HoldingSpaceCommandId::kPauseItem);
                   EXPECT_EQ(
                       event_source,
                       holding_space_metrics::EventSource::kHoldingSpaceItem);
                   paused_items.push_back(item);
                 }))}));
  }

  // Show UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Cache views.
  const std::vector<views::View*> views = test_api()->GetDownloadChips();
  ASSERT_EQ(views.size(), 2u);
  const std::vector<HoldingSpaceItemView*> item_views = {
      HoldingSpaceItemView::Cast(views[0]),
      HoldingSpaceItemView::Cast(views[1])};

  // Verify initial selection state.
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  // Move mouse to the 1st item.
  MoveMouseTo(item_views[0]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  // Select the 1st item.
  Click(item_views[0]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  {
    auto* primary_action =
        item_views[0]->GetViewByID(kHoldingSpaceItemPrimaryActionContainerId);
    ViewDrawnWaiter().Wait(primary_action);

    // Click the 1st item's primary action. Selection state shouldn't change.
    EXPECT_TRUE(cancelled_items.empty());
    Click(primary_action);
    EXPECT_THAT(cancelled_items, ElementsAre(item_views[0]->item()));
    EXPECT_TRUE(item_views[0]->selected());
    EXPECT_FALSE(item_views[1]->selected());

    // Reset tracking.
    cancelled_items.clear();
  }

  // Move mouse to the 2nd item.
  MoveMouseTo(item_views[1]);
  EXPECT_TRUE(item_views[0]->selected());
  EXPECT_FALSE(item_views[1]->selected());

  {
    auto* primary_action =
        item_views[1]->GetViewByID(kHoldingSpaceItemPrimaryActionContainerId);
    ViewDrawnWaiter().Wait(primary_action);

    // Click the 2nd item's primary action. Selection state should change.
    EXPECT_TRUE(cancelled_items.empty());
    Click(primary_action);
    EXPECT_THAT(cancelled_items, ElementsAre(item_views[1]->item()));
    EXPECT_FALSE(item_views[0]->selected());
    EXPECT_FALSE(item_views[1]->selected());
  }

  // Select the 2nd item.
  Click(item_views[1]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());

  {
    auto* secondary_action =
        item_views[1]->GetViewByID(kHoldingSpaceItemSecondaryActionContainerId);
    ViewDrawnWaiter().Wait(secondary_action);

    // Click the 2nd item's secondary action. Selection state shouldn't change.
    EXPECT_TRUE(paused_items.empty());
    Click(secondary_action);
    EXPECT_THAT(paused_items, ElementsAre(item_views[1]->item()));
    EXPECT_FALSE(item_views[0]->selected());
    EXPECT_TRUE(item_views[1]->selected());

    // Reset tracking.
    paused_items.clear();
  }

  // Move mouse to the 1st item.
  MoveMouseTo(item_views[0]);
  EXPECT_FALSE(item_views[0]->selected());
  EXPECT_TRUE(item_views[1]->selected());

  {
    auto* secondary_action =
        item_views[0]->GetViewByID(kHoldingSpaceItemSecondaryActionContainerId);
    ViewDrawnWaiter().Wait(secondary_action);

    // Click the 1st item's secondary action. Selection state should change.
    EXPECT_TRUE(paused_items.empty());
    Click(secondary_action);
    EXPECT_THAT(paused_items, ElementsAre(item_views[0]->item()));
    EXPECT_FALSE(item_views[0]->selected());
    EXPECT_FALSE(item_views[1]->selected());
  }
}

// Verifies that attempting to open holding space items via double click works
// as expected with event modifiers.
TEST_F(HoldingSpaceTrayTest, OpenItemsViaDoubleClickWithEventModifiers) {
  StartSession();

  // Add multiple holding space items.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));

  const auto show_holding_space_and_cache_views =
      [&](std::vector<HoldingSpaceItemView*>* item_views) {
        // Show holding space UI.
        test_api()->Show();
        ASSERT_TRUE(test_api()->IsShowing());

        // Cache holding space item views.
        const std::vector<views::View*> views =
            test_api()->GetPinnedFileChips();
        ASSERT_EQ(views.size(), 2u);
        *item_views = {HoldingSpaceItemView::Cast(views[0]),
                       HoldingSpaceItemView::Cast(views[1])};
      };

  std::vector<HoldingSpaceItemView*> item_views;
  show_holding_space_and_cache_views(&item_views);

  // Double click an item with the control key down. Expect the clicked holding
  // space item to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  DoubleClick(item_views[0], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Double click an item with the shift key down. Expect the clicked holding
  // space item to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  DoubleClick(item_views[0], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click the same item with the
  // control key down. Expect the clicked holding space item to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  Click(item_views[0]);
  DoubleClick(item_views[0], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click the same item with the
  // shift key down. Expect the clicked holding space item to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  Click(item_views[0]);
  DoubleClick(item_views[0], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click a different item with the
  // control key down. Expect both holding space items to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item(), item_views[1]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  Click(item_views[0]);
  DoubleClick(item_views[1], ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());

  // Reset.
  test_api()->Close();
  show_holding_space_and_cache_views(&item_views);

  // Click a holding space item. Then double click a different item with the
  // shift key down. Expect both holding space items to be opened.
  EXPECT_CALL(
      *client(),
      OpenItems(ElementsAre(item_views[0]->item(), item_views[1]->item()),
                Eq(holding_space_metrics::EventSource::kHoldingSpaceItem),
                /*callback=*/_));
  Click(item_views[0]);
  DoubleClick(item_views[1], ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(client());
}

// Verifies that holding space tray bubble closes after double clicking on a
// holding space item.
TEST_F(HoldingSpaceTrayTest, CloseTrayBubbleAfterDoubleClick) {
  StartSession();
  // Add a file to holding space.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));

  // Show and open the first item.
  test_api()->Show();
  std::vector<views::View*> pinned_file_chips =
      test_api()->GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  DoubleClick(pinned_file_chips[0]);

  // Wait for the tray bubble widget to be destroyed.
  views::test::WidgetDestroyedWaiter(test_api()->GetBubble()->GetWidget())
      .Wait();

  // Expect holding space tray bubble to be closed.
  EXPECT_FALSE(test_api()->IsShowing());
}

// Verifies that the holding space tray animates in and out as expected.
TEST_F(HoldingSpaceTrayTest, EnterAndExitAnimations) {
  // Ensure animations are run.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Prior to session start, the tray should not be showing.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  views::View* const tray = test_api()->GetTray();
  ASSERT_TRUE(tray && tray->layer());

  // Record transforms performed to the `tray` layer.
  ScopedTransformRecordingLayerDelegate transform_recorder(tray->layer());

  // Start the session. Because a holding space item was added in a previous
  // session (according to prefs state), the tray should show up without
  // animation.
  StartSession();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(tray->layer()->transform().IsIdentity());
  EXPECT_FALSE(transform_recorder.DidAnimate());
  transform_recorder.Reset();

  // Pin a holding space item. Because the tray was already showing there
  // should be no change in tray visibility.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake1"));
  MarkTimeOfFirstPin();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Because there was no change in visibility, there should be no transform.
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(tray->layer()->transform().IsIdentity());
  EXPECT_FALSE(transform_recorder.DidAnimate());
  transform_recorder.Reset();

  // Remove all holding space items. Because a holding space item was
  // previously pinned, the tray should animate out.
  RemoveAllItems();
  ViewVisibilityChangedWaiter().Wait(tray);
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The exit animation should be the default exit animation in which the tray
  // scales down and pivots about its center point.
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(transform_recorder.DidAnimate());
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.f, 1.f}, {0.5f, 0.5f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {11.f, 12.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Pin a holding space item. The tray should animate in.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // The entry animation should be the bounce in animation in which the tray
  // translates in vertically with scaling (since it previously scaled out).
  ui::LayerAnimationStoppedWaiter().Wait(tray->layer());
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(transform_recorder.DidAnimate());
  EXPECT_TRUE(transform_recorder.ScaledFrom({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({11.f, 12.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, -16.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Lock the screen. The tray should animate out.
  auto* session_controller =
      ash_test_helper()->test_session_controller_client();
  session_controller->SetSessionState(session_manager::SessionState::LOCKED);
  ViewVisibilityChangedWaiter().Wait(tray);
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // The exit animation should be the default exit animation in which the tray
  // scales down and pivots about its center point.
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(transform_recorder.DidAnimate());
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.0f, 1.0f}, {0.5f, 0.5f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {11.f, 12.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Unlock the screen. The tray should show up without animation.
  session_controller->UnlockScreen();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(tray->layer()->transform().IsIdentity());
  EXPECT_FALSE(transform_recorder.DidAnimate());
  transform_recorder.Reset();

  // Switch to another user with a populated model. The tray should show up
  // without animation.
  constexpr char kSecondaryUserId[] = "user@secondary";
  HoldingSpaceModel secondary_holding_space_model;
  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kPinnedFile,
                 base::FilePath("/tmp/fake3"));
  SwitchToSecondaryUser(kSecondaryUserId, /*client=*/nullptr,
                        &secondary_holding_space_model);
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // NOTE: When switching to the secondary user the tray will have briefly been
  // hidden while the primary user's holding space model was detached until the
  // secondary user's holding space model was attached. That said, the tray will
  // have scaled out and must scale back in but should *not* bounce.
  EXPECT_FALSE(tray->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(transform_recorder.ScaledFrom({1.f, 1.f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.ScaledInRange({0.5f, 0.5f}, {1.f, 1.f}));
  EXPECT_TRUE(transform_recorder.TranslatedFrom({0.f, 0.f}, {0.f, 0.f}));
  EXPECT_TRUE(transform_recorder.TranslatedInRange({0.f, 0.f}, {11.f, 12.f}));
  transform_recorder.Reset();

  // Clean up.
  UnregisterModelForUser(kSecondaryUserId);
}

// Verifies that the holding space bubble supports scrolling of pinned files.
TEST_F(HoldingSpaceTrayTest, SupportsScrollingOfPinnedFiles) {
  StartSession();

  // Show the holding space bubble.
  test_api()->Show();
  views::View* const pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);

  // Add batches of pinned files to holding space until the pinned files
  // bubble stops growing. Once the pinned files bubble has stopped growing, it
  // should be scrollable.
  for (size_t batch = 0u;; ++batch) {
    const int previous_height(pinned_files_bubble->height());

    for (size_t i = 0; i < 25u; ++i) {
      AddItem(HoldingSpaceItem::Type::kPinnedFile,
              base::FilePath(base::UnguessableToken().ToString()));
    }

    if (pinned_files_bubble->height() == previous_height)
      break;

    // Fail the test if the pinned files bubble does not overflow within a
    // reasonable number of batches.
    if (batch > 4u)
      GTEST_FAIL() << "Failed to overflow the pinned files bubble.";
  }

  // Add a suggested file so that the suggestions section will also be shown.
  AddItem(HoldingSpaceItem::Type::kLocalSuggestion,
          base::FilePath(base::UnguessableToken().ToString()));

  views::test::RunScheduledLayout(pinned_files_bubble->GetWidget());

  // Verify that the `pinned_files section` is completely contained within the
  // `pinned_files_bubble`.
  const auto* pinned_files_section =
      pinned_files_bubble->GetViewByID(kHoldingSpacePinnedFilesSectionId);
  ASSERT_TRUE(pinned_files_section);
  EXPECT_TRUE(pinned_files_bubble->GetContentsBounds().Contains(
      pinned_files_section->bounds()));

  // Verify that the `suggestions_section` is completely contained within the
  // `pinned_files_bubble`.
  const auto* suggestions_section =
      pinned_files_bubble->GetViewByID(kHoldingSpaceSuggestionsSectionId);
  ASSERT_TRUE(suggestions_section);
  EXPECT_TRUE(pinned_files_bubble->GetContentsBounds().Contains(
      suggestions_section->bounds()));

  // Verify that the `suggestions_section` appears below the
  // `pinned_files_section`.
  EXPECT_LT(pinned_files_section->bounds().bottom(),
            suggestions_section->bounds().y());

  // Cache the chips that were added to the pinned files bubble.
  const std::vector<views::View*> chips = test_api()->GetPinnedFileChips();
  ASSERT_GT(chips.size(), 0u);

  // Attempt to scroll the pinned files bubble and verify scroll success.
  const int previous_y = chips[0]->GetBoundsInScreen().y();
  GestureScrollBy(chips[0], /*offset_x=*/0, /*offset_y=*/-100);
  EXPECT_LT(chips[0]->GetBoundsInScreen().y(), previous_y);
}

TEST_F(HoldingSpaceTrayTest, HasExpectedBubbleTreatment) {
  StartSession();

  test_api()->Show();
  views::View* bubble = test_api()->GetBubble();
  ASSERT_TRUE(bubble);

  // Background.
  auto* background = bubble->GetBackground();
  ASSERT_TRUE(background);
  EXPECT_EQ(background->get_color(), SK_ColorTRANSPARENT);
  EXPECT_EQ(bubble->layer()->background_blur(), 0.f);

  // Border.
  EXPECT_FALSE(bubble->GetBorder());

  // Corner radius.
  EXPECT_FALSE(bubble->layer()->is_fast_rounded_corner());
  EXPECT_EQ(bubble->layer()->rounded_corner_radii(), gfx::RoundedCornersF(0.f));
}

TEST_F(HoldingSpaceTrayTest, CheckTrayAccessibilityText) {
  StartSession(/*pre_mark_time_of_first_add=*/true);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_EQ(GetTray()->GetAccessibleNameForTray(),
            u"Tote: recent screen captures, downloads, and pinned files");
}

TEST_F(HoldingSpaceTrayTest, TrayButtonWithRefreshIcon) {
  StartSession(/*pre_mark_time_of_first_add=*/true);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *test_api()->GetDefaultTrayIcon()->GetImage().bitmap(),
      *gfx::CreateVectorIcon(
           kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
           test_api()->GetDefaultTrayIcon()->GetColorProvider()->GetColor(
               kColorAshIconColorPrimary))
           .bitmap()));
}

TEST_F(HoldingSpaceTrayTest, CheckTrayTooltipText) {
  StartSession(/*pre_mark_time_of_first_add=*/true);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_EQ(GetTray()->GetTooltipText(gfx::Point()), u"Tote");
}

using HoldingSpacePreviewsTrayTest = HoldingSpaceTrayTestBase;

TEST_F(HoldingSpacePreviewsTrayTest, HideButtonOnChangeToEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();
  EnableTrayIconPreviews();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  HoldingSpaceModel secondary_holding_space_model;
  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  EnableTrayIconPreviews("user@secondary");

  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

TEST_F(HoldingSpacePreviewsTrayTest, HideButtonOnChangeToNonEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();
  EnableTrayIconPreviews();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  HoldingSpaceModel secondary_holding_space_model;
  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EnableTrayIconPreviews("user@secondary");

  EXPECT_FALSE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

// Tests that the tray icon size changes on in-app shelf.
TEST_F(HoldingSpacePreviewsTrayTest, UpdateTrayIconSizeForInAppShelf) {
  MarkTimeOfFirstPin();
  StartSession();
  EnableTrayIconPreviews();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_TRUE(test_api()->IsShowingInShelf());
  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());

  TabletModeControllerTestApi().EnterTabletMode();

  // Create a test widget to force in-app shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ASSERT_TRUE(widget);

  EXPECT_TRUE(test_api()->IsShowingInShelf());
  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconSmallPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());

  // Transition to home screen.
  widget->Minimize();

  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());
}

// Tests that the tray icon size changes on in-app shelf after transition from
// overview when overview is not showing in-app shelf.
TEST_F(
    HoldingSpacePreviewsTrayTest,
    UpdateTrayIconSizeForInAppShelfAfterTransitionFromOverviewWithHomeShelf) {
  MarkTimeOfFirstPin();
  StartSession();
  TabletModeControllerTestApi().EnterTabletMode();
  EnableTrayIconPreviews();

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  // Create a test widget and minimize it to transition to home screen.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ASSERT_TRUE(widget);
  widget->Minimize();

  ASSERT_FALSE(ShelfConfig::Get()->is_in_app());
  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());

  // Transition to overview, the shelf is expected to remain in home screen
  // style state.
  EnterOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);

  ASSERT_FALSE(ShelfConfig::Get()->is_in_app());
  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());

  // Tap the test window preview within the overview UI, and tap it to exit
  // overview.
  auto* overview_session = OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  auto* window = widget->GetNativeWindow();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window)->GetLeafItemForWindow(
          window);

  GetEventGenerator()->GestureTapAt(overview_item->overview_item_view()
                                        ->preview_view()
                                        ->GetBoundsInScreen()
                                        .CenterPoint());
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  ASSERT_TRUE(ShelfConfig::Get()->is_in_app());
  ASSERT_TRUE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));
  EXPECT_EQ(gfx::Size(kHoldingSpaceTrayIconSmallPreviewSize, kTrayItemSize),
            test_api()->GetPreviewsTrayIcon()->size());
}

// Tests that a shelf alignment change will behave as expected when there are
// multiple displays (and therefore multiple shelves/trays).
TEST_F(HoldingSpacePreviewsTrayTest, ShelfAlignmentChangeWithMultipleDisplays) {
  // This test requires multiple displays. Create two.
  UpdateDisplay("1280x768,1280x768");

  MarkTimeOfFirstPin();
  StartSession();
  EnableTrayIconPreviews();

  // Cache shelves/trays for each display.
  Shelf* const primary_shelf = GetShelf(GetPrimaryDisplay());
  Shelf* const secondary_shelf = GetShelf(GetSecondaryDisplay());
  HoldingSpaceTray* const primary_tray = GetTray(primary_shelf);
  HoldingSpaceTray* const secondary_tray = GetTray(secondary_shelf);

  // Trays should not initially be visible.
  ASSERT_FALSE(primary_tray->GetVisible());
  ASSERT_FALSE(secondary_tray->GetVisible());

  // Add a few holding space items to cause trays to show in shelves.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));

  // Trays should now be visible.
  ASSERT_TRUE(primary_tray->GetVisible());
  ASSERT_TRUE(secondary_tray->GetVisible());

  // Immediately update previews for each tray.
  primary_tray->FirePreviewsUpdateTimerIfRunningForTesting();
  secondary_tray->FirePreviewsUpdateTimerIfRunningForTesting();

  // Cache previews for each tray.
  views::View* const primary_icon_previews_container =
      primary_tray->GetViewByID(kHoldingSpaceTrayPreviewsIconId)->children()[0];
  views::View* const secondary_icon_previews_container =
      secondary_tray->GetViewByID(kHoldingSpaceTrayPreviewsIconId)
          ->children()[0];
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>&
      primary_icon_previews =
          primary_icon_previews_container->layer()->children();
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>&
      secondary_icon_previews =
          secondary_icon_previews_container->layer()->children();

  // Verify each tray contains three previews.
  ASSERT_EQ(primary_icon_previews.size(), 3u);
  ASSERT_EQ(secondary_icon_previews.size(), 3u);

  // Verify initial preview transforms. Since both shelves currently are bottom
  // aligned, previews should be positioned horizontally.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
  }

  // Change the secondary shelf to a vertical alignment.
  secondary_shelf->SetAlignment(ShelfAlignment::kRight);

  // Verify preview transforms. The primary shelf should still position its
  // previews horizontally but the secondary shelf should now position its
  // previews vertically.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(0, main_axis_offset));
  }

  // Change the secondary shelf back to a horizontal alignment.
  secondary_shelf->SetAlignment(ShelfAlignment::kBottom);

  // Verify preview transforms. Since both shelves are bottom aligned once
  // again, previews should be positioned horizontally.
  for (int i = 0; i < 3; ++i) {
    const int main_axis_offset =
        (2 - i) * kHoldingSpaceTrayIconDefaultPreviewSize / 2;
    ASSERT_EQ(primary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
    ASSERT_EQ(secondary_icon_previews[i]->transform().To2dTranslation(),
              gfx::Vector2d(main_axis_offset, 0));
  }
}

// Tests how screen captures section is updated during item addition, removal
// and initialization.
TEST_F(HoldingSpacePreviewsTrayTest, ScreenCapturesSection) {
  StartSession();
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add a screenshot item and verify recent file bubble gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_captures.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());

  // Add more items to fill up the section.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Fully initialize partially initialized item, and verify it gets added to
  // the section, in the order of addition, replacing the oldest item.
  model()->InitializeOrRemoveItem(
      item_2->id(), HoldingSpaceFile(item_2->file().file_path,
                                     HoldingSpaceFile::FileSystemType::kTest,
                                     GURL("filesystem:fake_2")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove other items, and verify the recent files bubble gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  model()->RemoveItem(item_3->id());
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the screen captures section is shown and orders items as expected
// when the model contains a number of initialized items prior to showing UI.
TEST_F(HoldingSpacePreviewsTrayTest,
       ScreenCapturesSectionWithInitializedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  const size_t max_screen_captures =
      GetMaxVisibleItemCount(HoldingSpaceSectionId::kScreenCaptures);

  // Add a number of initialized screen capture items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < max_screen_captures; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kScreenshot,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> screenshots = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(items.size(), screenshots.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* screenshot = HoldingSpaceItemView::Cast(screenshots.back());
    EXPECT_EQ(screenshot->item()->id(), items.front()->id());

    items.pop_front();
    screenshots.pop_back();
  }

  test_api()->Close();
}

TEST_F(HoldingSpacePreviewsTrayTest,
       InitializingScreenCaptureItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add enough screenshot items to fill up the section.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Fully initialize partially initialized item, and verify it's not added to
  // the section.
  model()->InitializeOrRemoveItem(
      item_1->id(), HoldingSpaceFile(item_1->file().file_path,
                                     HoldingSpaceFile::FileSystemType::kTest,
                                     GURL("filesystem:fake_1")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized screenshot item does not get shown if a
// fully initialized screenshot item gets removed from the holding space.
TEST_F(HoldingSpacePreviewsTrayTest,
       PartialItemNowShownOnRemovingAScreenCapture) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized item - verify it doesn't get shown in the UI yet.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_1"));

  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  test_api()->Close();
}

// Tests how the pinned item section is updated during item addition, removal
// and initialization.
TEST_F(HoldingSpacePreviewsTrayTest, PinnedFilesSection) {
  MarkTimeOfFirstPin();
  StartSession();

  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add a partially initialized item - verify it doesn't get shown in the UI
  // yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add more items to the section.
  HoldingSpaceItem* item_3 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Full initialize partially initialized item, and verify it gets shown.
  model()->InitializeOrRemoveItem(
      item_2->id(), HoldingSpaceFile(item_2->file().file_path,
                                     HoldingSpaceFile::FileSystemType::kTest,
                                     GURL("filesystem:fake_2")));

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove a partial item.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Remove other items, and verify the files section gets hidden.
  model()->RemoveItem(item_2->id());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
}

// Tests that as screen recording files are added to the model, they show in the
// screen captures section.
TEST_F(HoldingSpacePreviewsTrayTest,
       ScreenCapturesSectionWithScreenRecordingFiles) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add a screen recording item and verify recent files section gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenRecording,
                                     base::FilePath("/tmp/fake_1"));
  ASSERT_TRUE(item_1->IsInitialized());

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add a screenshot item, and verify it's also shown in the UI in the reverse
  // order they were added.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());

  // Remove the first item, and verify the section gets updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());

  test_api()->Close();
}

// Base class for tests of the holding space suggestions section parameterized
// by the set of holding space item types which are expected to appear there.
class HoldingSpaceTraySuggestionsSectionTest
    : public HoldingSpaceTrayTestBase,
      public ::testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  // Returns the holding space item type given the test parameterization.
  HoldingSpaceItem::Type GetType() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTraySuggestionsSectionTest,
    ::testing::Values(HoldingSpaceItem::Type::kDriveSuggestion,
                      HoldingSpaceItem::Type::kLocalSuggestion));

// Tests how the suggestions section is updated during item addition and
// removal.
TEST_P(HoldingSpaceTraySuggestionsSectionTest, SuggestionsSection) {
  StartSession();

  // Add an item to the suggestions section and verify that the pinned files
  // bubble shows that item.
  HoldingSpaceItem* item_1 = AddItem(GetType(), base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> suggestions = test_api()->GetSuggestionChips();
  ASSERT_EQ(1u, suggestions.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(suggestions[0])->item()->id());

  // Add another item and verify that the suggestions section is updated.
  HoldingSpaceItem* item_2 = AddItem(GetType(), base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  suggestions = test_api()->GetSuggestionChips();
  ASSERT_EQ(2u, suggestions.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(suggestions[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(suggestions[1])->item()->id());

  // Remove the newest item and verify that the suggestions section is updated.
  model()->RemoveItem(item_2->id());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  suggestions = test_api()->GetSuggestionChips();
  ASSERT_EQ(1u, suggestions.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(suggestions[0])->item()->id());

  // Remove the other item and verify that the suggestions section is empty.
  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
}

// Tests that suggestions are refreshed when showing holding space.
TEST_P(HoldingSpaceTraySuggestionsSectionTest, SuggestionsRefresh) {
  StartSession();

  // Show holding space.
  // Verify that `HoldingSpaceClient::RefreshSuggestions()` is called.
  EXPECT_CALL(*client(), RefreshSuggestions);
  test_api()->Show();
}

// Tests that suggestions can be removed via an item's context menu.
TEST_P(HoldingSpaceTraySuggestionsSectionTest, SuggestionsRemoval) {
  StartSession();

  // Add a suggestion item.
  const base::FilePath path("/tmp/fake_1");
  AddItem(GetType(), path);

  // Show holding space.
  test_api()->Show();
  std::vector<views::View*> item_views = test_api()->GetHoldingSpaceItemViews();
  ASSERT_EQ(item_views.size(), 1u);

  // Hover over the item view.
  MoveMouseTo(item_views.front());

  // Right click the item view to show the context menu.
  RightClick(item_views.front());

  // Click the menu item corresponding to item removal. Verify that
  // `HoldingSpaceClient::RemoveSuggestions()` is called.
  EXPECT_CALL(*client(),
              RemoveSuggestions(std::vector<base::FilePath>({path})));
  Click(GetMenuItemByCommandId(HoldingSpaceCommandId::kRemoveItem));
}

// Base class for tests of the holding space downloads section parameterized by
// the set of holding space item types which are expected to appear there.
class HoldingSpaceTrayDownloadsSectionTest
    : public HoldingSpaceTrayTestBase,
      public ::testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  // Returns the holding space item type given the test parameterization.
  HoldingSpaceItem::Type GetType() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTrayDownloadsSectionTest,
    ::testing::Values(HoldingSpaceItem::Type::kArcDownload,
                      HoldingSpaceItem::Type::kDiagnosticsLog,
                      HoldingSpaceItem::Type::kDownload,
                      HoldingSpaceItem::Type::kLacrosDownload,
                      HoldingSpaceItem::Type::kNearbyShare,
                      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
                      HoldingSpaceItem::Type::kPrintedPdf,
                      HoldingSpaceItem::Type::kScan));

// Tests how download chips are updated during item addition, removal and
// initialization.
TEST_P(HoldingSpaceTrayDownloadsSectionTest, DownloadsSection) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());

  // Add a download item and verify recent file bubble gets shown.
  std::vector<HoldingSpaceItem*> items;
  items.push_back(AddItem(GetType(), base::FilePath("/tmp/fake_1")));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  ASSERT_EQ(1u, test_api()->GetDownloadChips().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  items.push_back(
      AddPartiallyInitializedItem(GetType(), base::FilePath("/tmp/fake_2")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(items[0]->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  const size_t max_downloads =
      GetMaxVisibleItemCount(HoldingSpaceSectionId::kDownloads);

  // Add a few more download items until the section reaches capacity.
  for (size_t i = 2; i <= max_downloads; ++i) {
    items.push_back(AddItem(
        GetType(), base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(max_downloads, download_chips.size());

  // All downloads should be visible except for that which is associated with
  // the partially initialized item at index == `1`.
  for (int download_chip_index = 0, item_index = items.size() - 1;
       item_index >= 0; --item_index) {
    if (item_index != 1) {
      HoldingSpaceItemView* download_chip =
          HoldingSpaceItemView::Cast(download_chips.at(download_chip_index++));
      EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
    }
  }

  // Fully initialize partially initialized item, and verify it gets added to
  // the section, in the order of addition, replacing the oldest item.
  model()->InitializeOrRemoveItem(
      items[1]->id(), HoldingSpaceFile(items[1]->file().file_path,
                                       HoldingSpaceFile::FileSystemType::kTest,
                                       GURL("filesystem:fake_2")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();

  for (int download_chip_index = 0, item_index = items.size() - 1;
       item_index > 0; ++download_chip_index, --item_index) {
    HoldingSpaceItemView* download_chip =
        HoldingSpaceItemView::Cast(download_chips.at(download_chip_index));
    EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
  }

  // Remove the newest item, and verify the section gets updated.
  auto item_it = items.end() - 1;
  model()->RemoveItem((*item_it)->id());
  items.erase(item_it);

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(max_downloads, download_chips.size());

  for (int download_chip_index = 0, item_index = items.size() - 1;
       item_index >= 0; ++download_chip_index, --item_index) {
    HoldingSpaceItemView* download_chip =
        HoldingSpaceItemView::Cast(download_chips.at(download_chip_index));
    EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
  }

  // Remove other items and verify the recent files bubble gets hidden.
  while (!items.empty()) {
    model()->RemoveItem(items.front()->id());
    items.erase(items.begin());
  }

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the downloads section is shown and orders items as expected when the
// model contains a number of initialized items prior to showing UI.
TEST_P(HoldingSpaceTrayDownloadsSectionTest,
       DownloadsSectionWithInitializedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  const size_t max_downloads =
      GetMaxVisibleItemCount(HoldingSpaceSectionId::kDownloads);

  // Add a number of initialized download items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < max_downloads; ++i) {
    items.push_back(AddItem(
        GetType(), base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> download_files = test_api()->GetDownloadChips();
  ASSERT_EQ(items.size(), download_files.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* download_file = HoldingSpaceItemView::Cast(download_files.back());
    EXPECT_EQ(download_file->item()->id(), items.front()->id());

    items.pop_front();
    download_files.pop_back();
  }

  test_api()->Close();
}

TEST_P(HoldingSpaceTrayDownloadsSectionTest,
       InitializingDownloadItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  std::vector<HoldingSpaceItem*> items;
  items.push_back(
      AddPartiallyInitializedItem(GetType(), base::FilePath("/tmp/fake_1")));

  const size_t max_downloads =
      GetMaxVisibleItemCount(HoldingSpaceSectionId::kDownloads);

  // Add download items until the section reaches capacity.
  for (size_t i = 1; i < max_downloads + 1; ++i) {
    items.push_back(AddItem(
        GetType(), base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(max_downloads, download_chips.size());

  for (size_t download_chip_index = 0, item_index = items.size() - 1;
       item_index > 0; ++download_chip_index, --item_index) {
    HoldingSpaceItemView* download_chip =
        HoldingSpaceItemView::Cast(download_chips.at(download_chip_index));
    EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
  }

  // Fully initialize partially initialized item, and verify it's not added to
  // the section.
  model()->InitializeOrRemoveItem(
      items[0]->id(), HoldingSpaceFile(items[0]->file().file_path,
                                       HoldingSpaceFile::FileSystemType::kTest,
                                       GURL("filesystem:fake_1")));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(max_downloads, download_chips.size());

  for (size_t download_chip_index = 0, item_index = items.size() - 1;
       item_index > 0; ++download_chip_index, --item_index) {
    HoldingSpaceItemView* download_chip =
        HoldingSpaceItemView::Cast(download_chips.at(download_chip_index));
    EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
  }

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(items.front()->id());
  items.erase(items.begin());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(max_downloads, download_chips.size());

  for (int download_chip_index = 0, item_index = items.size() - 1;
       item_index >= 0; ++download_chip_index, --item_index) {
    HoldingSpaceItemView* download_chip =
        HoldingSpaceItemView::Cast(download_chips.at(download_chip_index));
    EXPECT_EQ(download_chip->item()->id(), items[item_index]->id());
  }
}

// Tests that a partially initialized download item does not get shown if a full
// download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayDownloadsSectionTest,
       PartialItemNowShownOnRemovingADownloadItem) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  AddPartiallyInitializedItem(GetType(), base::FilePath("/tmp/fake_1"));

  // Add two download items.
  HoldingSpaceItem* item_2 = AddItem(GetType(), base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(GetType(), base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetSuggestionChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
}

// Tests how opacity and transform for holding space tray's default tray icon is
// adjusted to avoid overlap with the holding space tray's progress indicator.
TEST_P(HoldingSpaceTrayDownloadsSectionTest,
       DefaultTrayIconOpacityAndTransform) {
  StartSession();

  // Cache `default_tray_icon`.
  views::View* const default_tray_icon =
      GetTray()->GetViewByID(kHoldingSpaceTrayDefaultIconId);
  ASSERT_TRUE(default_tray_icon);

  // Cache `progress_indicator`.
  ProgressIndicator* const progress_indicator = static_cast<ProgressIndicator*>(
      FindLayerWithName(GetTray(), ProgressIndicator::kClassName)->owner());
  ASSERT_TRUE(progress_indicator);

  // Wait until the `progress_indicator` is synced with the model, which happens
  // asynchronously in response to compositor scheduling.
  ASSERT_TRUE(RunUntil([&]() {
    return progress_indicator->progress() ==
           ProgressIndicator::kProgressComplete;
  }));

  // Verify initial opacity/transform.
  EXPECT_EQ(default_tray_icon->layer()->GetTargetOpacity(), 1.f);
  EXPECT_EQ(default_tray_icon->layer()->GetTargetTransform(), gfx::Transform());

  // Add an in-progress `item` to the model.
  HoldingSpaceItem* const item = AddItem(
      GetType(), base::FilePath("/tmp/fake_1"), HoldingSpaceProgress(0, 100));
  ASSERT_TRUE(item);

  // Wait until the `progress_indicator` is synced with the model. Note that
  // this happens asynchronously since the `progress_indicator` does so in
  // response to compositor scheduling.
  ASSERT_TRUE(
      RunUntil([&]() { return progress_indicator->progress() == 0.f; }));

  // The `default_tray_icon` should not be visible so as to avoid overlap with
  // the `progress_indicator`'s inner icon while in progress.
  EXPECT_EQ(default_tray_icon->layer()->GetTargetOpacity(), 0.f);
  EXPECT_EQ(default_tray_icon->layer()->GetTargetTransform(), gfx::Transform());

  // Complete the in-progress `item`.
  model()->UpdateItem(item->id())->SetProgress(HoldingSpaceProgress(100, 100));

  // Wait until the `progress_indicator` is synced with the model, which happens
  // asynchronously in response to compositor scheduling.
  ASSERT_TRUE(RunUntil([&]() {
    return progress_indicator->progress() ==
           ProgressIndicator::kProgressComplete;
  }));

  // Verify target opacity/transform.
  EXPECT_EQ(default_tray_icon->layer()->GetTargetOpacity(), 1.f);
  EXPECT_EQ(default_tray_icon->layer()->GetTargetTransform(), gfx::Transform());
}

// Tests how opacity and transform for holding space tray icon preview images
// are adjusted to avoid overlay with progress indicators.
TEST_P(HoldingSpaceTrayDownloadsSectionTest,
       TrayIconPreviewOpacityAndTransform) {
  StartSession();
  EnableTrayIconPreviews();

  // Add an in-progress `item` to the model.
  HoldingSpaceItem* const item = AddItem(
      GetType(), base::FilePath("/tmp/fake_1"), HoldingSpaceProgress(0, 100));
  ASSERT_TRUE(item);

  // Force immediate update of previews.
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  // Cache `preview`.
  ui::Layer* const preview =
      FindLayerWithName(GetTray(), HoldingSpaceTrayIconPreview::kClassName);
  ASSERT_TRUE(preview);

  // Cache `image`.
  ui::Layer* const image =
      FindLayerWithName(preview, HoldingSpaceTrayIconPreview::kImageLayerName);
  ASSERT_TRUE(image);

  // Cache `progress_indicator`.
  ProgressIndicator* const progress_indicator = static_cast<ProgressIndicator*>(
      FindLayerWithName(preview, ProgressIndicator::kClassName)->owner());
  ASSERT_TRUE(progress_indicator);

  // Wait until the `progress_indicator` is synced with the model, which happens
  // asynchronously in response to compositor scheduling.
  ASSERT_TRUE(
      RunUntil([&]() { return progress_indicator->progress() == 0.f; }));

  // Verify image opacity/transform.
  EXPECT_EQ(image->GetTargetOpacity(), 0.f);
  EXPECT_EQ(
      image->GetTargetTransform(),
      gfx::GetScaleTransform(gfx::Rect(image->size()).CenterPoint(), 0.7f));

  // Complete the in-progress `item`.
  model()->UpdateItem(item->id())->SetProgress(HoldingSpaceProgress(100, 100));

  // Wait until the `progress_indicator` is synced with the model, which happens
  // asynchronously in response to compositor scheduling.
  ASSERT_TRUE(RunUntil([&]() {
    return progress_indicator->progress() ==
           ProgressIndicator::kProgressComplete;
  }));

  // Verify image opacity.
  EXPECT_EQ(image->GetTargetOpacity(), 1.f);
  EXPECT_EQ(image->GetTargetTransform(), gfx::Transform());
}

// Tests that all expected progress indicator animations have animated when
// in-progress holding space items are added to the holding space model.
TEST_P(HoldingSpaceTrayDownloadsSectionTest, HasAnimatedProgressIndicators) {
  StartSession();
  EXPECT_TRUE(GetTray()->GetVisible());

  // Cache `prefs`.
  AccountId account_id = AccountId::FromUserEmail(kTestUser);
  auto* prefs = GetSessionControllerClient()->GetUserPrefService(account_id);
  ASSERT_TRUE(prefs);

  // Perform tests with previews shown/hidden.
  for (const auto& show_previews : {true, false}) {
    // Set previews enabled/disabled.
    holding_space_prefs::SetPreviewsEnabled(prefs, show_previews);
    EXPECT_EQ(holding_space_prefs::IsPreviewsEnabled(prefs), show_previews);

    // Create holding space `items`. Note that more holding space items are
    // being created than are visible at one time.
    std::vector<HoldingSpaceItem*> items;
    for (size_t i = 0; i <= kHoldingSpaceTrayIconMaxVisiblePreviews; ++i) {
      items.push_back(AddItem(
          GetType(), base::FilePath("/tmp/fake_" + base::NumberToString(i)),
          HoldingSpaceProgress(0, 100)));
    }

    // Update previews immediately.
    GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

    // Confirm expected tray icon visibility.
    EXPECT_EQ(test_api()->GetDefaultTrayIcon()->GetVisible(), !show_previews);
    EXPECT_EQ(test_api()->GetPreviewsTrayIcon()->GetVisible(), show_previews);

    // Cache `registry`.
    auto* registry = HoldingSpaceAnimationRegistry::GetInstance();
    ASSERT_TRUE(registry);

    // Confirm any expected icon animation for tray has started.
    auto* controller = HoldingSpaceController::Get();
    const auto controller_key =
        ProgressIndicatorAnimationRegistry::AsAnimationKey(controller);
    EXPECT_THAT(registry->GetProgressIconAnimationForKey(controller_key),
                Property(&ProgressIconAnimation::HasAnimated, IsTrue()));

    // Confirm all expected icon animations for `items` have started.
    for (const auto* item : items) {
      const auto item_key =
          ProgressIndicatorAnimationRegistry::AsAnimationKey(item);
      EXPECT_THAT(registry->GetProgressIconAnimationForKey(item_key),
                  Property(&ProgressIconAnimation::HasAnimated, IsTrue()));
    }
  }
}

class HoldingSpaceTraySuggestionsFeatureTest
    : public HoldingSpaceTrayTestBase,
      public ::testing::WithParamInterface</*suggestions_enabled=*/bool> {
 public:
  HoldingSpaceTraySuggestionsFeatureTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpaceSuggestions, IsHoldingSpaceSuggestionsEnabled());
  }

  void SetDisableDrive(bool disable) {
    ON_CALL(*client(), IsDriveDisabled).WillByDefault(testing::Return(disable));
  }

  bool IsHoldingSpaceSuggestionsEnabled() const { return GetParam(); }

  bool IsGoogleChromeBranded() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return true;
#else
    return false;
#endif
  }

  bool GSuiteIconsAreVisibleWhenSuggestionsFeatureIsEnabled(
      const views::View* pinned_files_bubble) const {
    bool has_icons = pinned_files_bubble->GetViewByID(
        kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId);
    bool should_have_icons =
        IsHoldingSpaceSuggestionsEnabled() && IsGoogleChromeBranded();
    return has_icons == should_have_icons;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceTraySuggestionsFeatureTest,
                         /*suggestions_enabled=*/testing::Bool());

TEST_P(HoldingSpaceTraySuggestionsFeatureTest,
       PinnedFilesPlaceholderShowsAfterPinUnpin) {
  StartSession(/*pre_mark_time_of_first_add=*/true);

  // The tray button should be shown because the user has previously added an
  // item to their holding space.
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  // Pin an item, then clear the model. The placeholder should not be shown.
  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstPin();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  RemoveAllItems();
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a downloaded file. Now the pinned placeholder should show if the
  // suggestions flag is enabled.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_EQ(test_api()->PinnedFilesBubbleShown(),
            IsHoldingSpaceSuggestionsEnabled());

  if (test_api()->PinnedFilesBubbleShown()) {
    views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
    ASSERT_TRUE(pinned_files_bubble);

    // If the suggestions feature is enabled, then the placeholder with the G
    // Suite icons should be showing without the Files app chip. Otherwise, the
    // placeholder shouldn't be showing at all.
    EXPECT_FALSE(pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId));
    EXPECT_TRUE(GSuiteIconsAreVisibleWhenSuggestionsFeatureIsEnabled(
        pinned_files_bubble));
  }
}

TEST_P(HoldingSpaceTraySuggestionsFeatureTest, TrayDoesNotShowUntilFirstAdd) {
  StartSession(/*pre_mark_time_of_first_add=*/false);

  // For the suggestions feature, the tray should still not show by default
  // in the shelf.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  MarkTimeOfFirstAdd();

  EXPECT_TRUE(test_api()->IsShowingInShelf());
}

// Until the user has pinned an item, a placeholder should exist in the pinned
// files bubble which contains a prompt to pin files and, in chrome branded
// builds, G Suite icons.
TEST_P(HoldingSpaceTraySuggestionsFeatureTest,
       PlaceholderContainsGSuitePrompt) {
  StartSession(/*pre_mark_time_of_first_add=*/true);

  // Show the bubble. Only the pinned files bubble should be visible.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // The new suggestions placeholder text and icons should exist in the pinned
  // files bubble.
  views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);

  views::Label* suggestions_placeholder_label =
      static_cast<views::Label*>(pinned_files_bubble->GetViewByID(
          kHoldingSpacePinnedFilesSectionPlaceholderLabelId));
  ASSERT_TRUE(suggestions_placeholder_label);

  std::u16string expected_text =
      IsHoldingSpaceSuggestionsEnabled()
          ? l10n_util::GetStringUTF16(
                IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT_SUGGESTIONS)
          : l10n_util::GetStringUTF16(
                IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT);
  EXPECT_EQ(suggestions_placeholder_label->GetText(), expected_text);

  // Also check to make sure that the label is adjusted when drive is disabled.
  test_api()->Close();
  SetDisableDrive(true);
  test_api()->Show();

  pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);

  suggestions_placeholder_label =
      static_cast<views::Label*>(pinned_files_bubble->GetViewByID(
          kHoldingSpacePinnedFilesSectionPlaceholderLabelId));
  ASSERT_TRUE(suggestions_placeholder_label);
  expected_text =
      IsHoldingSpaceSuggestionsEnabled()
          ? l10n_util::GetStringUTF16(
                IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT_SUGGESTIONS_DRIVE_DISABLED)
          : l10n_util::GetStringUTF16(
                IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT);
  EXPECT_EQ(suggestions_placeholder_label->GetText(), expected_text);

  bool has_files_app_chip =
      pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId);
  EXPECT_NE(has_files_app_chip, IsHoldingSpaceSuggestionsEnabled());
  EXPECT_TRUE(GSuiteIconsAreVisibleWhenSuggestionsFeatureIsEnabled(
      pinned_files_bubble));
}

// Base class for holding space tray tests which make assertions about primary
// and secondary actions on holding space item views. Tests are parameterized by
// holding space item type.
class HoldingSpaceTrayPrimaryAndSecondaryActionsTest
    : public HoldingSpaceTrayTestBase,
      public testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  // Returns the parameterized holding space item type.
  HoldingSpaceItem::Type GetType() const { return GetParam(); }

  // Returns whether the progress indicator inner icon is visible.
  bool IsProgressIndicatorInnerIconVisible(views::View* view) const {
    ui::Layer* progress_indicator_layer =
        FindLayerWithName(view, ProgressIndicator::kClassName);
    auto* progress_indicator =
        static_cast<ProgressIndicator*>(progress_indicator_layer->owner());
    return progress_indicator->inner_icon_visible();
  }

  // Returns whether the holding space image is currently showing.
  bool IsShowingImage(views::View* view) const {
    auto* v = view->GetViewByID(kHoldingSpaceItemImageId);
    return v && v->GetVisible();
  }

  // Returns whether a primary action is currently showing.
  bool IsShowingPrimaryAction(views::View* view) const {
    auto* v = view->GetViewByID(kHoldingSpaceItemPrimaryActionContainerId);
    return v && v->GetVisible();
  }

  // Returns whether a secondary action is currently showing.
  bool IsShowingSecondaryAction(views::View* view) const {
    auto* v = view->GetViewByID(kHoldingSpaceItemSecondaryActionContainerId);
    return v && v->GetVisible();
  }

  // Returns whether a context menu is showing with a command matching `id`.
  bool HasContextMenuCommand(HoldingSpaceCommandId id) const {
    return !!GetMenuItemByCommandId(id);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTrayPrimaryAndSecondaryActionsTest,
    testing::ValuesIn(holding_space_util::GetAllItemTypes()));

// Verifies that holding space item views have the expected primary and
// secondary actions for their state of progress, both inline and in their
// associated context menu.
TEST_P(HoldingSpaceTrayPrimaryAndSecondaryActionsTest, HasExpectedActions) {
  StartSession();

  // Create an in-progress holding space `item` of the parameterized type.
  HoldingSpaceItem* item = AddItem(GetType(), base::FilePath("/tmp/fake"),
                                   HoldingSpaceProgress(0, 100));

  // In-progress download items typically support in-progress commands.
  if (HoldingSpaceItem::IsDownloadType(item->type())) {
    EXPECT_TRUE(item->SetInProgressCommands(
        {CreateInProgressCommand(HoldingSpaceCommandId::kCancelItem,
                                 IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_CANCEL),
         CreateInProgressCommand(HoldingSpaceCommandId::kPauseItem,
                                 IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PAUSE)}));
  }

  // Show holding space UI.
  test_api()->Show();
  ASSERT_TRUE(test_api()->IsShowing());

  // Expect and cache a single holding space item view.
  std::vector<views::View*> item_views = test_api()->GetHoldingSpaceItemViews();
  ASSERT_EQ(item_views.size(), 1u);

  // Initially a primary and secondary action should not be shown as the holding
  // space item is not being hovered over.
  EXPECT_FALSE(IsShowingPrimaryAction(item_views.front()));
  EXPECT_FALSE(IsShowingSecondaryAction(item_views.front()));

  if (!HoldingSpaceItem::IsScreenCaptureType(item->type())) {
    // For non-screen capture items, the inner icon of the progress indicator
    // should be shown when the secondary action container is hidden.
    EXPECT_TRUE(IsProgressIndicatorInnerIconVisible(item_views.front()));
    // The holding space image should only be shown if the secondary action
    // container is hidden.
    EXPECT_FALSE(IsShowingImage(item_views.front()));
  } else {
    // For screen capture items, the holding space image should always be shown.
    EXPECT_TRUE(IsShowingImage(item_views.front()));
  }

  // Hover over the item view.
  MoveMouseTo(item_views.front());

  // Expect a primary and secondary action to be shown only for download type
  // holding space items. In-progress items of other types do not currently
  // support primary and secondary actions.
  EXPECT_EQ(IsShowingPrimaryAction(item_views.front()),
            HoldingSpaceItem::IsDownloadType(item->type()));
  EXPECT_EQ(IsShowingSecondaryAction(item_views.front()),
            HoldingSpaceItem::IsDownloadType(item->type()));

  if (!HoldingSpaceItem::IsScreenCaptureType(item->type())) {
    // For non-screen capture items, the inner icon of the progress indicator
    // should only be shown if the secondary action container is hidden.
    EXPECT_NE(IsProgressIndicatorInnerIconVisible(item_views.front()),
              IsShowingSecondaryAction(item_views.front()));
    // The holding space image should only be shown if the secondary action
    // container is hidden.
    EXPECT_FALSE(IsShowingImage(item_views.front()));
  } else {
    // For screen capture items, the holding space image should always be shown.
    EXPECT_TRUE(IsShowingImage(item_views.front()));
  }

  // Right click the item view to show the context menu.
  RightClick(item_views.front());
  EXPECT_TRUE(IsShowingContextMenu());

  // Verify context menu commands for in-progress holding space items.
  for (const HoldingSpaceCommandId& id : GetHoldingSpaceCommandIds()) {
    bool expect_context_menu_command = false;
    switch (id) {
      case HoldingSpaceCommandId::kShowInFolder:
        expect_context_menu_command = true;
        break;
      case HoldingSpaceCommandId::kCancelItem:
      case HoldingSpaceCommandId::kPauseItem:
        expect_context_menu_command =
            HoldingSpaceItem::IsDownloadType(item->type());
        break;
      default:
        // No action necessary.
        break;
    }
    EXPECT_EQ(HasContextMenuCommand(id), expect_context_menu_command);
  }

  // Press and release ESC to close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  EXPECT_FALSE(IsShowingContextMenu());

  // Hide progress for the holding space `item`.
  model()
      ->UpdateItem(item->id())
      ->SetProgress(
          HoldingSpaceProgress(0, 100, /*complete=*/false, /*hidden=*/true));

  // Hover over the tray.
  MoveMouseTo(GetTray());

  // When not hovered over, images should be shown for holding space items with
  // hidden progress since progress indication will not be shown.
  EXPECT_TRUE(IsShowingImage(item_views.front()));

  // Complete the holding space `item`.
  model()->UpdateItem(item->id())->SetProgress(HoldingSpaceProgress(100, 100));

  // Hover over the item view.
  MoveMouseTo(item_views.front());

  // Expect only a primary action to be shown for completed items.
  EXPECT_TRUE(IsShowingPrimaryAction(item_views.front()));
  EXPECT_FALSE(IsShowingSecondaryAction(item_views.front()));

  // Holding space images should always be shown for completed items.
  EXPECT_TRUE(IsShowingImage(item_views.front()));

  // Right click the item view to show the context menu.
  RightClick(item_views.front());
  EXPECT_TRUE(IsShowingContextMenu());

  // Verify context menu commands for completed holding space items.
  for (const HoldingSpaceCommandId& id : GetHoldingSpaceCommandIds()) {
    bool expect_context_menu_command = false;
    switch (id) {
      case HoldingSpaceCommandId::kPinItem:
        expect_context_menu_command =
            item->type() != HoldingSpaceItem::Type::kPinnedFile;
        break;
      case HoldingSpaceCommandId::kRemoveItem:
        expect_context_menu_command =
            item->type() != HoldingSpaceItem::Type::kPinnedFile;
        break;
      case HoldingSpaceCommandId::kShowInFolder:
        expect_context_menu_command = true;
        break;
      case HoldingSpaceCommandId::kUnpinItem:
        expect_context_menu_command =
            item->type() == HoldingSpaceItem::Type::kPinnedFile;
        break;
      default:
        // No action necessary.
        break;
    }
    EXPECT_EQ(HasContextMenuCommand(id), expect_context_menu_command);
  }
}

// TODO(crbug.com/1373911): Once `HoldingSpaceTrayTest` is smaller,
// parameterized, and based on `HoldingSpaceAshTestBase`, this can be folded
// into it. Test suite to confirm that holding space is visible in the tray when
// appropriate.
class HoldingSpaceTrayVisibilityTest
    : public HoldingSpaceAshTestBase,
      public testing::WithParamInterface<
          std::tuple<HoldingSpaceItem::Type,
                     /*suggestions_enabled=*/bool>> {
 public:
  HoldingSpaceTrayVisibilityTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpaceSuggestions, IsHoldingSpaceSuggestionsEnabled());
  }

  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();
    test_api_ = std::make_unique<HoldingSpaceTestApi>();
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  // Returns the parameterized holding space item type.
  HoldingSpaceItem::Type GetType() const { return std::get<0>(GetParam()); }

  bool IsHoldingSpaceSuggestionsEnabled() const {
    return std::get<1>(GetParam());
  }

  HoldingSpaceTestApi* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTrayVisibilityTest,
    testing::Combine(testing::ValuesIn(holding_space_util::GetAllItemTypes()),
                     /*suggestions_enabled=*/testing::Bool()));

TEST_P(HoldingSpaceTrayVisibilityTest, TrayShowsForCorrectItemTypes) {
  // Partially initialized items should not cause the tray to show.
  HoldingSpaceItem* item =
      AddPartiallyInitializedItem(GetType(), base::FilePath("/tmp/fake"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Once initialized, the item should show the tray if appropriate.
  model()->InitializeOrRemoveItem(
      item->id(),
      HoldingSpaceFile(
          item->file().file_path, HoldingSpaceFile::FileSystemType::kTest,
          GURL(base::StrCat(
              {"filesystem:", item->file().file_path.BaseName().value()}))));

    // A suggestion alone should not show the tray.
    EXPECT_NE(test_api()->IsShowingInShelf(),
              HoldingSpaceItem::IsSuggestionType(GetType()));
}

}  // namespace ash
