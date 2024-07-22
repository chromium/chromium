// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/component_extension_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

namespace {

constexpr int kMainContainerWidth = 296;
constexpr int kMainContainerCornerRadius = 24;

constexpr int kHeaderBottomMargin = 16;
constexpr float kAddContainerCornerRadius = 16.0f;
constexpr float kAddButtonCornerRadius = 10.0f;
// This is associated to the size of `ash::IconButton::Type::kMedium`.
constexpr int kIconButtonSize = 32;

// Gap from focus ring outer edge to the edge of the view.
constexpr float kFocusRingHaloInset = -4.0f;
// Thickness of focus ring.
constexpr float kFocusRingHaloThickness = 2.0f;

// Space for focus ring of the list item.
constexpr int kSpaceForFocusRing = 1 - kFocusRingHaloInset;

// Move the space of `kSpaceForFocusRing` to `scroll_content_` so the focus ring
// will not be cut for the top and bottom list item.
constexpr int kAddRowBottomMargin = 8 - kSpaceForFocusRing;

constexpr char kKeyEditNudgeID[] = "kGameControlsKeyEditNudge";
constexpr char kHelpUrl[] =
    "https://support.google.com/chromebook/?p=game-controls-help";

void UpdateFocusRingOnThemeChanged(views::Button* button) {
  // Set up highlight and focus ring for `button`.
  ash::StyleUtil::SetUpInkDropForButton(
      /*button=*/button, gfx::Insets(), /*highlight_on_hover=*/false,
      /*highlight_on_focus=*/false);

  // `StyleUtil::SetUpInkDropForButton()` reinstalls the focus ring, so it
  // needs to set the focus ring size after calling
  // `StyleUtil::SetUpInkDropForButton()`.
  auto* focus_ring = views::FocusRing::Get(button);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetHaloThickness(kFocusRingHaloThickness);
}

}  // namespace

// -----------------------------------------------------------------------------
// EditingList::AddContainerButton:

// +-----------------------------------+
// ||"Create (your first) button"|  |+||
// +-----------------------------------+
class EditingList::AddContainerButton : public views::Button {
  METADATA_HEADER(AddContainerButton, views::Button)

 public:
  explicit AddContainerButton(base::RepeatingCallback<void()> callback)
      : views::Button(callback) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insets=*/gfx::Insets(),
        /*between_child_spacing=*/12));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 16)));
    UpdateBackground(/*add_background=*/true);
    SetNotifyEnterExitOnChild(true);

    // Add title.
    title_ = AddChildView(
        ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton2, u"",
                                       cros_tokens::kCrosSysOnSurface));
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 12));
    // `+` button should be right aligned, so flex label to fill empty space.
    layout->SetFlexForView(title_, /*flex=*/1);
    title_changed_callback_ =
        title_->AddTextChangedCallback(base::BindRepeating(
            &AddContainerButton::OnTitleChanged, base::Unretained(this)));

    // Add `add_button_` and apply design style.
    add_button_ = AddChildView(std::make_unique<views::LabelButton>(callback));
    // Ignore `add_button_` for the screen reader.
    add_button_->GetViewAccessibility().SetIsIgnored(true);
    // Never focus on `add_button_` since it is redundant with the
    // `AddContainerButton` (b/331643468).
    add_button_->SetFocusBehavior(FocusBehavior::NEVER);
    add_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysPrimary, kAddButtonCornerRadius));
    add_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
    add_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kGameControlsAddIcon,
                                       cros_tokens::kCrosSysOnPrimary,
                                       /*icon_size=*/20));
    add_button_->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::ImageModel::FromVectorIcon(kGameControlsAddIcon,
                                       cros_tokens::kCrosSysInverseOnSurface,
                                       /*icon_size=*/20));
    add_button_->SetImageCentered(true);

    // Set up focus ring.
    views::HighlightPathGenerator::Install(
        this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), kAddContainerCornerRadius));
    UpdateFocusRingOnThemeChanged(this);
  }

  AddContainerButton(const AddContainerButton&) = delete;
  AddContainerButton& operator=(const AddContainerButton) = delete;
  ~AddContainerButton() override = default;

  // Updates the background. If `add_background` is true, add
  // a default background. Otherwise, remove the background.
  void UpdateBackground(bool add_background) {
    // No need to update the background if there is an expected background.
    if (add_background == !!GetBackground()) {
      return;
    }

    SetBackground(add_background
                      ? views::CreateThemedRoundedRectBackground(
                            GetState() != ButtonState::STATE_DISABLED
                                ? cros_tokens::kCrosSysSystemOnBase
                                : cros_tokens::kCrosSysInverseOnSurface,
                            kAddContainerCornerRadius)
                      : nullptr);
  }

  void UpdateTitle(bool is_zero_state) {
    DCHECK(title_);
    title_->SetText(l10n_util::GetStringUTF16(
        is_zero_state ? IDS_INPUT_OVERLAY_EDITING_LIST_FIRST_CONTROL_LABEL
                      : IDS_INPUT_OVERLAY_EDITING_LIST_NEW_CONTROL_LABEL));
    GetViewAccessibility().SetName(title_->GetText());
  }

  void UpdateAddButtonState(size_t current_controls_size) {
    const ButtonState state = current_controls_size < kMaxActionCount
                                  ? ButtonState::STATE_NORMAL
                                  : ButtonState::STATE_DISABLED;
    if (state == GetState()) {
      return;
    }

    add_button_->SetState(state);
    SetState(state);
  }

  views::LabelButton* add_button() { return add_button_; }

 private:
  void OnTitleChanged() {
    CHECK(add_button_);
    add_button_->SetTooltipText(title_->GetText());
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    bool enabled = GetState() != ButtonState::STATE_DISABLED;
    title_->SetEnabledColorId(enabled ? cros_tokens::kCrosSysOnSurface
                                      : cros_tokens::kCrosSysDisabled);
    if (auto* scroll_view = views::AsViewClass<views::ScrollView>(parent())) {
      UpdateBackground(/*add_background=*/scroll_view->GetVisibleRect().y() ==
                       0);
    }
    add_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        enabled ? cros_tokens::kCrosSysPrimary : cros_tokens::kCrosSysDisabled,
        kAddButtonCornerRadius));
  }

  // Owned by views hierarchy.
  raw_ptr<views::Label> title_;
  raw_ptr<views::LabelButton> add_button_;

  base::CallbackListSubscription title_changed_callback_;
};

BEGIN_METADATA(EditingList, AddContainerButton)
END_METADATA

// -----------------------------------------------------------------------------
// EditingList:

EditingList::EditingList(DisplayOverlayController* controller)
    : TouchInjectorObserver(), controller_(controller) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

EditingList::~EditingList() {
  controller_->RemoveTouchInjectorObserver(this);
}

void EditingList::UpdateWidget() {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->UpdateWidgetBoundsInRootWindow(
      widget, gfx::Rect(GetWidgetMagneticPositionLocal(), GetPreferredSize()));
}

void EditingList::Init() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque,
      kMainContainerCornerRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kMainContainerCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical,
                       gfx::Insets::VH(kEditingListInsideBorderInsets, 0)))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  AddHeader();
  add_container_ =
      AddChildView(std::make_unique<AddContainerButton>(base::BindRepeating(
          &EditingList::OnAddButtonPressed, base::Unretained(this))));
  add_container_->UpdateAddButtonState(controller_->GetActiveActionsSize());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetBackgroundColor(std::nullopt);
  on_scroll_view_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &EditingList::OnScrollViewScrolled, base::Unretained(this)));
  scroll_content_ = scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_content_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  // Add contents.
  if (HasControls()) {
    AddControlListContent();
  } else {
    UpdateOnZeroState(/*is_zero_state=*/true);
  }

  SizeToPreferredSize();

  shadow_ = ash::SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, ash::SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kMainContainerCornerRadius);
}

bool EditingList::HasControls() const {
  DCHECK(controller_);
  return controller_->GetActiveActionsSize() != 0u;
}

void EditingList::AddHeader() {
  // +-----------------------------------+
  // ||"Controls"|    |? button| |"Done"||
  // +-----------------------------------+
  auto* header_container = AddChildView(std::make_unique<views::View>());
  auto* layout =
      header_container->SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  header_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kEditingListInsideBorderInsets, kHeaderBottomMargin,
                        kEditingListInsideBorderInsets));

  // Add header title.
  editing_header_label_ =
      header_container->AddChildView(ash::bubble_utils::CreateLabel(
          ash::TypographyToken::kCrosTitle1,
          l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDITING_LIST_TITLE),
          cros_tokens::kCrosSysOnSurface));

  editing_header_label_->SetProperty(views::kMarginsKey,
                                     gfx::Insets::TLBR(0, 0, 0, 32));
  editing_header_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Buttons should be right aligned, so flex label to fill empty space.
  layout->SetFlexForView(editing_header_label_, /*flex=*/1);

  // Add helper button.
  auto* help_button =
      header_container->AddChildView(std::make_unique<ash::IconButton>(
          base::BindRepeating(&EditingList::OnHelpButtonPressed,
                              base::Unretained(this)),
          ash::IconButton::Type::kMedium, &ash::kGdHelpIcon,
          IDS_INPUT_OVERLAY_EDITING_LIST_HELP_BUTTON_NAME));
  help_button->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 8));

  // Add done button.
  done_button_ =
      header_container->AddChildView(std::make_unique<ash::PillButton>(
          base::BindRepeating(&EditingList::OnDoneButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_INPUT_OVERLAY_EDITING_DONE_BUTTON_LABEL),
          ash::PillButton::Type::kSecondaryWithoutIcon));
  done_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_INPUT_OVERLAY_EDITING_LIST_DONE_BUTTON_A11Y_LABEL));
}

void EditingList::AddControlListContent() {
  UpdateOnZeroState(/*is_zero_state=*/false);

  // Add list content as:
  // --------------------------
  // | ---------------------- |
  // | | ActionViewListItem | |
  // | ---------------------- |
  // | ---------------------- |
  // | | ActionViewListItem | |
  // | ---------------------- |
  // | ......                 |
  // --------------------------
  // TODO(b/270969479): Wrap `scroll_content` in a scroll view.
  DCHECK(controller_);
  DCHECK(scroll_content_);
  for (const auto& action : controller_->touch_injector()->actions()) {
    if (action->IsDeleted()) {
      continue;
    }
    scroll_content_->AddChildView(
        std::make_unique<ActionViewListItem>(controller_, action.get()));
  }
}

void EditingList::MaybeApplyEduDecoration() {
  // Show education decoration only once.
  if (const auto& list_children = scroll_content_->children();
      show_edu_ && list_children.size() == 1u) {
    ShowKeyEditNudge();
    PerformPulseAnimation();
    show_edu_ = false;
  }
}

void EditingList::ShowKeyEditNudge() {
  const auto& list_children = scroll_content_->children();
  DCHECK_EQ(list_children.size(), 1u);

  auto nudge_data = ash::AnchoredNudgeData(
      kKeyEditNudgeID, ash::NudgeCatalogName::kGameDashboardControlsNudge,
      l10n_util::GetStringUTF16(
          IDS_INPUT_OVERLAY_EDITING_LIST_KEY_EDIT_NUDGE_SUB_TITLE),
      list_children[0]);
  nudge_data.title_text = l10n_util::GetStringUTF16(
      IDS_INPUT_OVERLAY_EDITING_LIST_KEY_EDIT_NUDGE_TITLE);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_ARC_INPUT_OVERLAY_KEY_EDIT_NUDGE_JSON);
  nudge_data.background_color_id = cros_tokens::kCrosSysBaseHighlight;
  nudge_data.image_background_color_id = cros_tokens::kCrosSysOnBaseHighlight;
  nudge_data.arrow = views::BubbleBorder::LEFT_CENTER;
  nudge_data.duration = ash::NudgeDuration::kMediumDuration;
  ash::Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
}

void EditingList::PerformPulseAnimation() {
  const auto& scroll_children = scroll_content_->children();
  DCHECK_EQ(scroll_children.size(), 1u);
  if (auto* list_item =
          views::AsViewClass<ActionViewListItem>(scroll_children[0])) {
    list_item->PerformPulseAnimation();
  }
}

void EditingList::UpdateOnZeroState(bool is_zero_state) {
  is_zero_state_ = is_zero_state;

  CHECK(add_container_);
  add_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kEditingListInsideBorderInsets,
                        is_zero_state_ ? 0 : kAddRowBottomMargin,
                        kEditingListInsideBorderInsets));

  add_container_->UpdateTitle(is_zero_state_);

  // Add extra space on the vertical border to ensure the focus ring is not cut
  // off for the top and bottom list item.
  CHECK(scroll_content_);
  scroll_content_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      is_zero_state ? 0 : kSpaceForFocusRing, kEditingListInsideBorderInsets)));
}

void EditingList::OnAddButtonPressed() {
  // TODO(b/304819827): Support action type choose.
  DCHECK(scroll_content_);
  // Key edit nudge only shows up after adding the first action.
  if (scroll_content_->children().size() == 1u) {
    ash::Shell::Get()->anchored_nudge_manager()->Cancel(kKeyEditNudgeID);
  }
  controller_->EnterButtonPlaceMode(ActionType::TAP);
  RecordEditingListFunctionTriggered(controller_->GetPackageName(),
                                     EditingListFunction::kAdd);
}

void EditingList::OnDoneButtonPressed() {
  DCHECK(controller_);
  RecordEditingListFunctionTriggered(controller_->GetPackageName(),
                                     EditingListFunction::kDone);
  controller_->OnCustomizeSave();
}

void EditingList::OnHelpButtonPressed() {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kHelpUrl), ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void EditingList::UpdateScrollView(bool scroll_to_bottom) {
  scroll_view_->InvalidateLayout();
  if (scroll_to_bottom) {
    scroll_view_->ScrollByOffset(
        gfx::PointF(0, scroll_content_->GetPreferredSize().height()));
  }

  UpdateWidget();
  add_container_->UpdateBackground(
      /*add_background=*/!HasScrollOffset());
}

void EditingList::OnScrollViewScrolled() {
  add_container_->UpdateBackground(/*add_background=*/!HasScrollOffset());
}

bool EditingList::HasScrollOffset() {
  return scroll_view_->GetVisibleRect().y() != 0;
}

void EditingList::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
}

void EditingList::OnDragUpdate(const ui::LocatedEvent& event) {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->RemoveDeleteEditShortcutWidget();
  auto widget_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
  widget_bounds.Offset(
      /*horizontal=*/(event.location() - start_drag_event_pos_).x(),
      /*vertical=*/0);
  widget->SetBounds(widget_bounds);
}

void EditingList::OnDragEnd(const ui::LocatedEvent& event) {
  UpdateWidget();
}

gfx::Point EditingList::GetWidgetMagneticPositionLocal() {
  auto* widget = GetWidget();
  DCHECK(widget);

  const int width = GetPreferredSize().width();
  const auto anchor_bounds = controller_->touch_injector()->content_bounds();
  const auto available_bounds = CalculateAvailableBounds(
      controller_->touch_injector()->window()->GetRootWindow());

  // Check if there is space on left and right side outside of the sibling game
  // window.
  bool has_space_on_left =
      anchor_bounds.x() - width - kEditingListSpaceBetweenMainWindow >= 0;
  bool has_space_on_right =
      anchor_bounds.right() + width + kEditingListSpaceBetweenMainWindow <
      available_bounds.width();

  // Check if the attached widget should be inside or outside of the attached
  // sibling game window.
  bool should_be_outside = has_space_on_left || has_space_on_right;

  // Check if the attached widget should be on left or right side of the
  // attached sibling game window.
  auto center = widget->GetNativeWindow()->bounds().CenterPoint();
  auto anchor_center = anchor_bounds.CenterPoint();
  bool should_be_on_left = center.x() < anchor_center.x();
  // Prefer to have the attached widget outside of the sibling game window if
  // there is enough space on left or right. Otherwise, apply the attached
  // widget inside of the sibling game window.
  bool on_left_side =
      (should_be_outside &&
       ((has_space_on_left && should_be_on_left) || !has_space_on_right)) ||
      (!should_be_outside && should_be_on_left);

  // Calculate attached widget origin in root window.
  gfx::Point window_origin = anchor_bounds.origin();
  if (on_left_side) {
    window_origin.SetPoint(
        should_be_outside
            ? anchor_bounds.x() - width - kEditingListSpaceBetweenMainWindow
            : anchor_bounds.x() + kEditingListOffsetInsideMainWindow,
        should_be_outside
            ? window_origin.y()
            : window_origin.y() + kEditingListOffsetInsideMainWindow);
  } else {
    window_origin.SetPoint(
        should_be_outside
            ? anchor_bounds.right() + kEditingListSpaceBetweenMainWindow
            : anchor_bounds.right() - width -
                  kEditingListOffsetInsideMainWindow,
        should_be_outside
            ? window_origin.y()
            : window_origin.y() + kEditingListOffsetInsideMainWindow);
  }

  ClipScrollViewHeight(should_be_outside);

  return window_origin;
}

ActionViewListItem* EditingList::GetListItemForAction(Action* action) {
  const auto& list_items = scroll_content_->children();
  auto it =
      std::find_if(list_items.begin(), list_items.end(), [&](const auto& p) {
        if (auto* list_item = views::AsViewClass<ActionViewListItem>(p)) {
          return action == list_item->action();
        } else {
          return false;
        }
      });

  return it == list_items.end() ? nullptr
                                : views::AsViewClass<ActionViewListItem>(*it);
}

void EditingList::ClipScrollViewHeight(bool is_outside) {
  int max_height = controller_->touch_injector()->content_bounds().height() -
                   add_container_->GetPreferredSize().height() -
                   2 * kEditingListInsideBorderInsets - kHeaderBottomMargin -
                   kAddRowBottomMargin - kIconButtonSize;
  if (!is_outside) {
    max_height -= kEditingListOffsetInsideMainWindow;
  }

  scroll_view_->ClipHeightTo(/*min_height=*/0, /*max_height=*/max_height);
}

gfx::Size EditingList::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMainContainerWidth,
                   GetLayoutManager()->GetPreferredHeightForWidth(
                       this, kMainContainerWidth));
}

bool EditingList::OnMousePressed(const ui::MouseEvent& event) {
  OnDragStart(event);
  return true;
}

bool EditingList::OnMouseDragged(const ui::MouseEvent& event) {
  OnDragUpdate(event);
  return true;
}

void EditingList::OnMouseReleased(const ui::MouseEvent& event) {
  OnDragEnd(event);
}

void EditingList::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollUpdate:
      OnDragUpdate(*event);
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      OnDragEnd(*event);
      event->SetHandled();
      break;
    default:
      return;
  }
}
void EditingList::VisibilityChanged(views::View* starting_from,
                                    bool is_visible) {
  if (is_visible) {
    MaybeApplyEduDecoration();
  }
}

void EditingList::OnActionAdded(Action& action) {
  DCHECK(scroll_content_);
  const size_t active_action_size = controller_->GetActiveActionsSize();
  if (active_action_size == 1u) {
    // Clear the zero-state.
    UpdateOnZeroState(/*is_zero_state=*/false);
    show_edu_ = true;
  }
  scroll_content_->AddChildView(
      std::make_unique<ActionViewListItem>(controller_, &action));
  // Scroll the list to bottom when a new action is added.
  UpdateScrollView(/*scroll_to_bottom=*/true);

  add_container_->UpdateAddButtonState(active_action_size);
}

void EditingList::OnActionRemoved(const Action& action) {
  DCHECK(scroll_content_);
  for (views::View* child : scroll_content_->children()) {
    auto* list_item = views::AsViewClass<ActionViewListItem>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      scroll_content_->RemoveChildViewT(list_item);
      UpdateScrollView(/*scroll_to_bottom=*/false);
      break;
    }
  }
  // Set to zero-state if it is empty.
  if (controller_->GetActiveActionsSize() == 0u) {
    UpdateOnZeroState(/*is_zero_state=*/true);
  }

  add_container_->UpdateAddButtonState(controller_->GetActiveActionsSize());
}

void EditingList::OnActionTypeChanged(Action* action, Action* new_action) {
  DCHECK(!is_zero_state_);
  for (size_t i = 0; i < scroll_content_->children().size(); i++) {
    if (auto* list_item = views::AsViewClass<ActionViewListItem>(
            scroll_content_->children()[i]);
        list_item && list_item->action() == action) {
      scroll_content_->RemoveChildViewT(list_item);
      scroll_content_->AddChildViewAt(
          std::make_unique<ActionViewListItem>(controller_, new_action), i);
      UpdateScrollView(/*scroll_to_bottom=*/false);
      return;
    }
  }
}

void EditingList::OnActionInputBindingUpdated(const Action& action) {
  DCHECK(scroll_content_);
  for (views::View* child : scroll_content_->children()) {
    auto* list_item = views::AsViewClass<ActionViewListItem>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      list_item->OnActionInputBindingUpdated();
      break;
    }
  }
}

void EditingList::OnActionNewStateRemoved(const Action& action) {
  DCHECK(scroll_content_);
  for (views::View* child : scroll_content_->children()) {
    if (auto* list_item = views::AsViewClass<ActionViewListItem>(child);
        list_item && list_item->action() == &action) {
      list_item->RemoveNewState();
      break;
    }
  }
}

bool EditingList::IsKeyEditNudgeShownForTesting() const {
  return ash::Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      kKeyEditNudgeID);
}

ash::AnchoredNudge* EditingList::GetKeyEditNudgeForTesting() const {
  return ash::Shell::Get()
      ->anchored_nudge_manager()
      ->GetShownNudgeForTest(  // IN-TEST
          kKeyEditNudgeID);
}

views::LabelButton* EditingList::GetAddButtonForTesting() const {
  return add_container_->add_button();
}

views::Button* EditingList::GetAddContainerButtonForTesting() const {
  return views::AsViewClass<views::Button>(add_container_);
}

BEGIN_METADATA(EditingList)
END_METADATA

}  // namespace arc::input_overlay
