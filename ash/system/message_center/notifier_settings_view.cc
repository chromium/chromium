// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notifier_settings_view.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

using message_center::MessageCenter;
using message_center::NotifierId;
using ContentLayerType = AshColorProvider::ContentLayerType;
using AshColorMode = AshColorProvider::AshColorMode;

namespace {

const int kNotifierButtonWrapperHeight = 48;
const int kHorizontalMargin = 12;
const int kEntryIconSize = 20;
const int kInternalHorizontalSpacing = 16;
const int kSmallerInternalHorizontalSpacing = 12;
const int kCheckboxSizeWithPadding = 28;

// The width of the settings pane in pixels.
const int kWidth = 360;

// The minimum height of the settings pane in pixels.
const int kMinimumHeight = 480;

// Checkboxes have some built-in right padding blank space.
const int kInnateCheckboxRightPadding = 2;

// Spec defines the checkbox size; the innate padding throws this measurement
// off so we need to compute a slightly different area for the checkbox to
// inhabit.
constexpr int kComputedCheckboxSize =
    kCheckboxSizeWithPadding - kInnateCheckboxRightPadding;

// TODO(tetsui): Give more general names and remove kEntryHeight, etc.
constexpr gfx::Insets kTopLabelPadding(16, 18, 15, 18);
const int kQuietModeViewSpacing = 18;

constexpr gfx::Insets kHeaderViewPadding(4, 0);
constexpr gfx::Insets kQuietModeViewPadding(0, 18, 0, 0);
constexpr gfx::Insets kQuietModeLabelPadding(16, 0, 15, 0);
constexpr gfx::Insets kQuietModeTogglePadding(0, 14);
constexpr SkColor kTopBorderColor = SkColorSetA(SK_ColorBLACK, 0x1F);
const int kLabelFontSizeDelta = 1;

// NotifierButtonWrapperView ---------------------------------------------------

// A wrapper view of NotifierButton to guarantee the fixed height
// |kNotifierButtonWrapperHeight|. The button is placed in the middle of
// the wrapper view by giving padding to the top and the bottom.
// The view is focusable and provides focus painter. When the button is disabled
// (NotifierMetadata.enforced), it also applies filter to make the color of the
// button dim.
class NotifierButtonWrapperView : public views::View {
 public:
  explicit NotifierButtonWrapperView(views::View* contents);
  ~NotifierButtonWrapperView() override;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;
  const char* GetClassName() const override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;

  // NotifierButton to wrap.
  views::View* contents_;

  DISALLOW_COPY_AND_ASSIGN(NotifierButtonWrapperView);
};

NotifierButtonWrapperView::NotifierButtonWrapperView(views::View* contents)
    : focus_painter_(TrayPopupUtils::CreateFocusPainter()),
      contents_(contents) {
  AddChildView(contents);
}

NotifierButtonWrapperView::~NotifierButtonWrapperView() = default;

void NotifierButtonWrapperView::Layout() {
  int contents_width = width();
  int contents_height = contents_->GetHeightForWidth(contents_width);
  int y = std::max((height() - contents_height) / 2, 0);
  contents_->SetBounds(0, y, contents_width, contents_height);

  SetFocusBehavior(contents_->GetEnabled() ? FocusBehavior::ALWAYS
                                           : FocusBehavior::NEVER);
}

gfx::Size NotifierButtonWrapperView::CalculatePreferredSize() const {
  return gfx::Size(kWidth, kNotifierButtonWrapperHeight);
}

void NotifierButtonWrapperView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  contents_->GetAccessibleNodeData(node_data);
}

void NotifierButtonWrapperView::OnFocus() {
  views::View::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
  // We render differently when focused.
  SchedulePaint();
}

bool NotifierButtonWrapperView::OnKeyPressed(const ui::KeyEvent& event) {
  return contents_->OnKeyPressed(event);
}

bool NotifierButtonWrapperView::OnKeyReleased(const ui::KeyEvent& event) {
  return contents_->OnKeyReleased(event);
}

bool NotifierButtonWrapperView::OnMousePressed(const ui::MouseEvent& event) {
  return contents_->OnMousePressed(event);
}

void NotifierButtonWrapperView::OnMouseReleased(const ui::MouseEvent& event) {
  contents_->OnMouseReleased(event);
}

void NotifierButtonWrapperView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void NotifierButtonWrapperView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

const char* NotifierButtonWrapperView::GetClassName() const {
  return "NotifierButtonWrapperView";
}

// ScrollContentsView ----------------------------------------------------------

class ScrollContentsView : public views::View {
 public:
  ScrollContentsView() = default;

  // views::View:
  const char* GetClassName() const override { return "ScrollContentsView"; }

 private:
  void PaintChildren(const views::PaintInfo& paint_info) override {
    views::View::PaintChildren(paint_info);

    if (y() == 0)
      return;

    // Draw a shadow at the top of the viewport when scrolled.
    const ui::PaintContext& context = paint_info.context();
    gfx::Rect shadowed_area(0, 0, width(), -y());

    ui::PaintRecorder recorder(context, size());
    gfx::Canvas* canvas = recorder.canvas();
    gfx::ShadowValues shadow;
    shadow.emplace_back(
        gfx::Vector2d(0, message_center_style::kScrollShadowOffsetY),
        message_center_style::kScrollShadowBlur,
        message_center_style::kScrollShadowColor);
    cc::PaintFlags flags;
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setAntiAlias(true);
    canvas->ClipRect(shadowed_area, SkClipOp::kDifference);
    canvas->DrawRect(shadowed_area, flags);
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollContentsView);
};

// EmptyNotifierView -----------------------------------------------------------

class EmptyNotifierView : public views::View {
 public:
  EmptyNotifierView() {
    const SkColor text_color =
        AshColorProvider::Get()->DeprecatedGetContentLayerColor(
            ContentLayerType::kTextPrimary, kUnifiedMenuTextColor);
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    views::ImageView* icon = new views::ImageView();
    icon->SetImage(gfx::CreateVectorIcon(kNotificationCenterEmptyIcon,
                                         message_center_style::kEmptyIconSize,
                                         text_color));
    icon->SetBorder(
        views::CreateEmptyBorder(message_center_style::kEmptyIconPadding));
    AddChildView(icon);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_NO_NOTIFIERS));
    label->SetEnabledColor(text_color);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    // "Roboto-Medium, 12sp" is specified in the mock.
    label->SetFontList(
        gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    AddChildView(label);
  }

  // views::View:
  const char* GetClassName() const override { return "EmptyNotifierView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyNotifierView);
};

}  // namespace

// NotifierSettingsView::NotifierButton ---------------------------------------

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
NotifierSettingsView::NotifierButton::NotifierButton(
    const NotifierMetadata& notifier,
    views::ButtonListener* listener)
    : views::Button(listener), notifier_id_(notifier.notifier_id) {
  auto icon_view = std::make_unique<views::ImageView>();
  auto name_view = std::make_unique<views::Label>(notifier.name);
  auto checkbox =
      std::make_unique<views::Checkbox>(base::string16(), this /* listener */);
  name_view->SetAutoColorReadabilityEnabled(false);
  name_view->SetEnabledColor(
      AshColorProvider::Get()->DeprecatedGetContentLayerColor(
          ContentLayerType::kTextPrimary, kUnifiedMenuTextColor));
  name_view->SetSubpixelRenderingEnabled(false);
  // "Roboto-Regular, 13sp" is specified in the mock.
  name_view->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));

  checkbox->SetChecked(notifier.enabled);
  checkbox->SetFocusBehavior(FocusBehavior::NEVER);
  checkbox->SetAccessibleName(notifier.name);

  if (notifier.enforced) {
    Button::SetEnabled(false);
    checkbox->SetEnabled(false);

    icon_view->SetPaintToLayer();
    icon_view->layer()->SetFillsBoundsOpaquely(false);
    icon_view->layer()->SetOpacity(gfx::kDisabledControlAlpha / float{0xFF});
    name_view->SetEnabledColor(
        SkColorSetA(name_view->GetEnabledColor(), gfx::kDisabledControlAlpha));
  }

  // Add the views before the layout is assigned. Because GridChanged() may be
  // called multiple times, these views should already be child views.
  checkbox_ = AddChildView(std::move(checkbox));
  icon_view_ = AddChildView(std::move(icon_view));
  name_view_ = AddChildView(std::move(name_view));

  UpdateIconImage(notifier.icon);
}

NotifierSettingsView::NotifierButton::~NotifierButton() = default;

void NotifierSettingsView::NotifierButton::UpdateIconImage(
    const gfx::ImageSkia& icon) {
  if (icon.isNull()) {
    icon_view_->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kEntryIconSize,
        AshColorProvider::Get()->GetContentLayerColor(
            ContentLayerType::kIconPrimary, AshColorMode::kDark)));
  } else {
    icon_view_->SetImage(icon);
    icon_view_->SetImageSize(gfx::Size(kEntryIconSize, kEntryIconSize));
  }
  GridChanged();
}

void NotifierSettingsView::NotifierButton::SetChecked(bool checked) {
  checkbox_->SetChecked(checked);
}

bool NotifierSettingsView::NotifierButton::GetChecked() const {
  return checkbox_->GetChecked();
}

const char* NotifierSettingsView::NotifierButton::GetClassName() const {
  return "NotifierButton";
}

void NotifierSettingsView::NotifierButton::ButtonPressed(
    views::Button* button,
    const ui::Event& event) {
  DCHECK_EQ(button, checkbox_);
  // The checkbox state has already changed at this point, but we'll update
  // the state on NotifierSettingsView::ButtonPressed() too, so here change
  // back to the previous state.
  checkbox_->SetChecked(!checkbox_->GetChecked());
  Button::NotifyClick(event);
}

void NotifierSettingsView::NotifierButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  static_cast<views::View*>(checkbox_)->GetAccessibleNodeData(node_data);
}

void NotifierSettingsView::NotifierButton::GridChanged() {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());
  ColumnSet* cs = layout->AddColumnSet(0);
  // Add a column for the checkbox.
  cs->AddPaddingColumn(0, kInnateCheckboxRightPadding);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                kComputedCheckboxSize, 0);
  cs->AddPaddingColumn(0, kInternalHorizontalSpacing);

  // Add a column for the icon.
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                kEntryIconSize, 0);
  cs->AddPaddingColumn(0, kSmallerInternalHorizontalSpacing);

  // Add a column for the name.
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  // Add a padding column which contains expandable blank space.
  cs->AddPaddingColumn(1, 0);

  layout->StartRow(0, 0);
  layout->AddExistingView(checkbox_);
  layout->AddExistingView(icon_view_);
  layout->AddExistingView(name_view_);

  if (!GetEnabled()) {
    auto policy_enforced_icon = std::make_unique<views::ImageView>();
    policy_enforced_icon->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon, kEntryIconSize,
        AshColorProvider::Get()->GetContentLayerColor(
            ContentLayerType::kIconPrimary, AshColorMode::kDark)));
    cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                  kEntryIconSize, 0);
    layout->AddView(std::move(policy_enforced_icon));
  }

  Layout();
}

// NotifierSettingsView -------------------------------------------------------

NotifierSettingsView::NotifierSettingsView() {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto header_view = std::make_unique<views::View>();
  header_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHeaderViewPadding, 0));
  header_view->SetBorder(
      views::CreateSolidSidedBorder(1, 0, 0, 0, kTopBorderColor));

  auto quiet_mode_view = std::make_unique<views::View>();

  auto* quiet_mode_layout =
      quiet_mode_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kQuietModeViewPadding,
          kQuietModeViewSpacing));

  auto quiet_mode_icon = std::make_unique<views::ImageView>();
  quiet_mode_icon->SetBorder(views::CreateEmptyBorder(kQuietModeLabelPadding));
  quiet_mode_icon_ = quiet_mode_view->AddChildView(std::move(quiet_mode_icon));

  auto quiet_mode_label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  const SkColor text_color =
      AshColorProvider::Get()->DeprecatedGetContentLayerColor(
          ContentLayerType::kTextPrimary, kUnifiedMenuTextColor);
  quiet_mode_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // "Roboto-Regular, 13sp" is specified in the mock.
  quiet_mode_label->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));
  quiet_mode_label->SetAutoColorReadabilityEnabled(false);
  quiet_mode_label->SetEnabledColor(text_color);
  quiet_mode_label->SetSubpixelRenderingEnabled(false);
  quiet_mode_label->SetBorder(views::CreateEmptyBorder(kQuietModeLabelPadding));
  auto* quiet_mode_label_ptr =
      quiet_mode_view->AddChildView(std::move(quiet_mode_label));
  quiet_mode_layout->SetFlexForView(quiet_mode_label_ptr, 1);

  auto quiet_mode_toggle = std::make_unique<views::ToggleButton>(this);
  quiet_mode_toggle->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  quiet_mode_toggle->SetBorder(
      views::CreateEmptyBorder(kQuietModeTogglePadding));
  quiet_mode_toggle->EnableCanvasFlippingForRTLUI(true);
  quiet_mode_toggle_ =
      quiet_mode_view->AddChildView(std::move(quiet_mode_toggle));
  SetQuietModeState(MessageCenter::Get()->IsQuietMode());
  header_view->AddChildView(std::move(quiet_mode_view));

  auto top_label = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
  top_label->SetBorder(views::CreateEmptyBorder(kTopLabelPadding));
  // "Roboto-Medium, 13sp" is specified in the mock.
  top_label->SetFontList(gfx::FontList().Derive(
      kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  top_label->SetAutoColorReadabilityEnabled(false);
  top_label->SetEnabledColor(text_color);
  top_label->SetSubpixelRenderingEnabled(false);
  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label->SetMultiLine(true);
  top_label_ = header_view->AddChildView(std::move(top_label));

  header_view_ = AddChildView(std::move(header_view));

  auto scroller = std::make_unique<views::ScrollView>();
  scroller->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroll_bar_ = scroller->SetVerticalScrollBar(
      std::make_unique<views::OverlayScrollBar>(/*horizontal=*/false));
  scroller->SetDrawOverflowIndicator(false);
  scroller_ = AddChildView(std::move(scroller));

  no_notifiers_view_ = AddChildView(std::make_unique<EmptyNotifierView>());

  OnNotifiersUpdated({});
  NotifierSettingsController::Get()->AddNotifierSettingsObserver(this);
  NotifierSettingsController::Get()->GetNotifiers();
}

NotifierSettingsView::~NotifierSettingsView() {
  NotifierSettingsController::Get()->RemoveNotifierSettingsObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_toggle_->SetIsOn(is_quiet_mode);
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kIconPrimary, AshColorMode::kDark);
  if (is_quiet_mode) {
    quiet_mode_icon_->SetImage(gfx::CreateVectorIcon(
        kNotificationCenterDoNotDisturbOnIcon, kMenuIconSize, icon_color));
  } else {
    quiet_mode_icon_->SetImage(gfx::CreateVectorIcon(
        kNotificationCenterDoNotDisturbOffIcon, kMenuIconSize,
        AshColorProvider::GetDisabledColor(icon_color)));
  }
}

void NotifierSettingsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kList;
  node_data->SetName(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
}

const char* NotifierSettingsView::GetClassName() const {
  return "NotifierSettingsView";
}

void NotifierSettingsView::OnNotifiersUpdated(
    const std::vector<NotifierMetadata>& notifiers) {
  // TODO(tetsui): currently notifier settings list doesn't update after once
  // it's loaded, in order to retain scroll position.
  if (scroller_->contents() && buttons_.size() > 0)
    return;

  buttons_.clear();

  auto contents_view = std::make_unique<ScrollContentsView>();
  contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kHorizontalMargin)));

  for (const auto& notifier : notifiers) {
    NotifierButton* button = new NotifierButton(notifier, this);
    NotifierButtonWrapperView* wrapper = new NotifierButtonWrapperView(button);

    wrapper->SetFocusBehavior(FocusBehavior::ALWAYS);
    contents_view->AddChildView(wrapper);
    buttons_.insert(button);
  }

  top_label_->SetVisible(!buttons_.empty());
  no_notifiers_view_->SetVisible(buttons_.empty());
  top_label_->InvalidateLayout();

  auto* contents_view_ptr = scroller_->SetContents(std::move(contents_view));

  contents_view_ptr->SetBoundsRect(
      gfx::Rect(contents_view_ptr->GetPreferredSize()));
  Layout();
}

void NotifierSettingsView::OnNotifierIconUpdated(const NotifierId& notifier_id,
                                                 const gfx::ImageSkia& icon) {
  for (auto* button : buttons_) {
    if (button->notifier_id() == notifier_id) {
      button->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::Layout() {
  int original_scroll_position = scroller_->GetVisibleRect().y();
  int header_height = header_view_->GetHeightForWidth(width());
  header_view_->SetBounds(0, 0, width(), header_height);

  views::View* contents_view = scroller_->contents();
  int content_width = width();
  int content_height = contents_view->GetHeightForWidth(content_width);
  if (header_height + content_height > height()) {
    content_width -= scroller_->GetScrollBarLayoutWidth();
    content_height = contents_view->GetHeightForWidth(content_width);
  }
  contents_view->SetBounds(0, 0, content_width, content_height);
  scroller_->SetBounds(0, header_height, width(), height() - header_height);
  no_notifiers_view_->SetBounds(0, header_height, width(),
                                height() - header_height);

  // The scroll position may have changed after the layout.
  scroller_->ScrollToPosition(scroll_bar_, original_scroll_position);
}

gfx::Size NotifierSettingsView::GetMinimumSize() const {
  gfx::Size size(kWidth, kMinimumHeight);
  int total_height = header_view_->GetPreferredSize().height() +
                     scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumHeight)
    size.Enlarge(scroller_->GetScrollBarLayoutWidth(), 0);
  return size;
}

gfx::Size NotifierSettingsView::CalculatePreferredSize() const {
  gfx::Size header_size = header_view_->GetPreferredSize();
  gfx::Size content_size = scroller_->contents()->GetPreferredSize();
  int no_notifiers_height = 0;
  if (no_notifiers_view_->GetVisible())
    no_notifiers_height = no_notifiers_view_->GetPreferredSize().height();
  return gfx::Size(
      std::max(header_size.width(), content_size.width()),
      std::max(kMinimumHeight, header_size.height() + content_size.height() +
                                   no_notifiers_height));
}

bool NotifierSettingsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    GetWidget()->Close();
    return true;
  }

  return scroller_->OnKeyPressed(event);
}

bool NotifierSettingsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return scroller_->OnMouseWheel(event);
}

void NotifierSettingsView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  if (sender == quiet_mode_toggle_) {
    MessageCenter::Get()->SetQuietMode(quiet_mode_toggle_->GetIsOn());
    return;
  }

  auto iter = buttons_.find(static_cast<NotifierButton*>(sender));
  if (iter == buttons_.end())
    return;

  NotifierButton* button = *iter;
  button->SetChecked(!button->GetChecked());
  NotifierSettingsController::Get()->SetNotifierEnabled(button->notifier_id(),
                                                        button->GetChecked());
  NotifierSettingsController::Get()->GetNotifiers();
}

}  // namespace ash
