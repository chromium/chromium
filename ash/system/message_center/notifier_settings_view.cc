// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notifier_settings_view.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
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
using mojom::NotifierUiData;
using message_center::NotifierId;

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
constexpr SkColor kDisabledNotifierFilterColor =
    SkColorSetA(SK_ColorWHITE, 0xB8);
const int kLabelFontSizeDelta = 1;

// NotifierButtonWrapperView ---------------------------------------------------

// A wrapper view of NotifierButton to guarantee the fixed height
// |kNotifierButtonWrapperHeight|. The button is placed in the middle of
// the wrapper view by giving padding to the top and the bottom.
// The view is focusable and provides focus painter. When the button is disabled
// (NotifierUiData.enforced), it also applies filter to make the color of the
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
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;

 private:
  // Initialize |disabled_filter_|. Should be called once.
  void CreateDisabledFilter();

  std::unique_ptr<views::Painter> focus_painter_;

  // NotifierButton to wrap.
  views::View* contents_;

  // A view to add semi-transparent filter on top of |contents_|.
  // It is only visible when NotifierButton is disabled (e.g. the setting is
  // enforced by administrator.) The color of the NotifierButton would be dim
  // and users notice they can't change the setting.
  views::View* disabled_filter_ = nullptr;

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

  // Since normally we don't show |disabled_filter_|, initialize it lazily.
  if (!contents_->enabled()) {
    if (!disabled_filter_)
      CreateDisabledFilter();
    disabled_filter_->SetVisible(true);
    gfx::Rect filter_bounds = GetContentsBounds();
    filter_bounds.set_width(filter_bounds.width() - kEntryIconSize);
    disabled_filter_->SetBoundsRect(filter_bounds);
  } else if (disabled_filter_) {
    disabled_filter_->SetVisible(false);
  }

  SetFocusBehavior(contents_->enabled() ? FocusBehavior::ALWAYS
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

void NotifierButtonWrapperView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void NotifierButtonWrapperView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void NotifierButtonWrapperView::CreateDisabledFilter() {
  DCHECK(!disabled_filter_);
  disabled_filter_ = new views::View;
  disabled_filter_->SetBackground(
      views::CreateSolidBackground(kDisabledNotifierFilterColor));
  disabled_filter_->set_can_process_events_within_subtree(false);
  AddChildView(disabled_filter_);
}

// ScrollContentsView ----------------------------------------------------------

class ScrollContentsView : public views::View {
 public:
  ScrollContentsView() = default;

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
    SkColor color = kUnifiedMenuTextColor;
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(), 0);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(std::move(layout));

    views::ImageView* icon = new views::ImageView();
    icon->SetImage(gfx::CreateVectorIcon(kNotificationCenterEmptyIcon,
                                         message_center_style::kEmptyIconSize,
                                         color));
    icon->SetBorder(
        views::CreateEmptyBorder(message_center_style::kEmptyIconPadding));
    AddChildView(icon);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_NO_NOTIFIERS));
    label->SetEnabledColor(color);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    // "Roboto-Medium, 12sp" is specified in the mock.
    label->SetFontList(
        gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    AddChildView(label);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyNotifierView);
};

}  // namespace

// NotifierSettingsView::NotifierButton ---------------------------------------

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
NotifierSettingsView::NotifierButton::NotifierButton(
    const mojom::NotifierUiData& notifier_ui_data,
    views::ButtonListener* listener)
    : views::Button(listener),
      notifier_id_(notifier_ui_data.notifier_id),
      icon_view_(new views::ImageView()),
      name_view_(new views::Label(notifier_ui_data.name)),
      checkbox_(new views::Checkbox(base::string16(), this /* listener */)) {
  name_view_->SetAutoColorReadabilityEnabled(false);
  name_view_->SetEnabledColor(kUnifiedMenuTextColor);
  name_view_->SetSubpixelRenderingEnabled(false);
  // "Roboto-Regular, 13sp" is specified in the mock.
  name_view_->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));

  checkbox_->SetChecked(notifier_ui_data.enabled);
  checkbox_->SetFocusBehavior(FocusBehavior::NEVER);
  checkbox_->SetAccessibleName(notifier_ui_data.name);

  if (notifier_ui_data.enforced) {
    Button::SetEnabled(false);
    checkbox_->SetEnabled(false);
  }

  UpdateIconImage(notifier_ui_data.icon);
}

NotifierSettingsView::NotifierButton::~NotifierButton() = default;

void NotifierSettingsView::NotifierButton::UpdateIconImage(
    const gfx::ImageSkia& icon) {
  if (icon.isNull()) {
    icon_view_->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kEntryIconSize, kUnifiedMenuIconColor));
  } else {
    icon_view_->SetImage(icon);
    icon_view_->SetImageSize(gfx::Size(kEntryIconSize, kEntryIconSize));
  }
  GridChanged();
}

void NotifierSettingsView::NotifierButton::SetChecked(bool checked) {
  checkbox_->SetChecked(checked);
}

bool NotifierSettingsView::NotifierButton::checked() const {
  return checkbox_->checked();
}

void NotifierSettingsView::NotifierButton::ButtonPressed(
    views::Button* button,
    const ui::Event& event) {
  DCHECK_EQ(button, checkbox_);
  // The checkbox state has already changed at this point, but we'll update
  // the state on NotifierSettingsView::ButtonPressed() too, so here change
  // back to the previous state.
  checkbox_->SetChecked(!checkbox_->checked());
  Button::NotifyClick(event);
}

void NotifierSettingsView::NotifierButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  static_cast<views::View*>(checkbox_)->GetAccessibleNodeData(node_data);
}

void NotifierSettingsView::NotifierButton::GridChanged() {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>(this));
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
  layout->AddView(checkbox_);
  layout->AddView(icon_view_);
  layout->AddView(name_view_);

  if (!enabled()) {
    views::ImageView* policy_enforced_icon = new views::ImageView();
    policy_enforced_icon->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon, kEntryIconSize, kUnifiedMenuIconColor));
    cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                  kEntryIconSize, 0);
    layout->AddView(policy_enforced_icon);
  }

  Layout();
}

// NotifierSettingsView -------------------------------------------------------

NotifierSettingsView::NotifierSettingsView()
    : title_arrow_(nullptr),
      quiet_mode_icon_(nullptr),
      quiet_mode_toggle_(nullptr),
      header_view_(nullptr),
      top_label_(nullptr),
      scroller_(nullptr),
      no_notifiers_view_(nullptr) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  header_view_ = new views::View;
  header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, kHeaderViewPadding, 0));
  header_view_->SetBorder(
      views::CreateSolidSidedBorder(1, 0, 0, 0, kTopBorderColor));

  views::View* quiet_mode_view = new views::View;

  auto* quiet_mode_layout =
      quiet_mode_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, kQuietModeViewPadding,
          kQuietModeViewSpacing));

  quiet_mode_icon_ = new views::ImageView();
  quiet_mode_icon_->SetBorder(views::CreateEmptyBorder(kQuietModeLabelPadding));
  quiet_mode_view->AddChildView(quiet_mode_icon_);

  views::Label* quiet_mode_label = new views::Label(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  quiet_mode_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // "Roboto-Regular, 13sp" is specified in the mock.
  quiet_mode_label->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));
  quiet_mode_label->SetAutoColorReadabilityEnabled(false);
  quiet_mode_label->SetEnabledColor(kUnifiedMenuTextColor);
  quiet_mode_label->SetSubpixelRenderingEnabled(false);
  quiet_mode_label->SetBorder(views::CreateEmptyBorder(kQuietModeLabelPadding));
  quiet_mode_view->AddChildView(quiet_mode_label);
  quiet_mode_layout->SetFlexForView(quiet_mode_label, 1);

  quiet_mode_toggle_ = new views::ToggleButton(this);
  quiet_mode_toggle_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  quiet_mode_toggle_->SetBorder(
      views::CreateEmptyBorder(kQuietModeTogglePadding));
  quiet_mode_toggle_->EnableCanvasFlippingForRTLUI(true);
  SetQuietModeState(MessageCenter::Get()->IsQuietMode());
  quiet_mode_view->AddChildView(quiet_mode_toggle_);
  header_view_->AddChildView(quiet_mode_view);

  top_label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
  top_label_->SetBorder(views::CreateEmptyBorder(kTopLabelPadding));
  // "Roboto-Medium, 13sp" is specified in the mock.
  top_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  top_label_->SetAutoColorReadabilityEnabled(false);
  top_label_->SetEnabledColor(kUnifiedMenuTextColor);
  top_label_->SetSubpixelRenderingEnabled(false);
  top_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label_->SetMultiLine(true);
  header_view_->AddChildView(top_label_);

  AddChildView(header_view_);

  scroller_ = new views::ScrollView();
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->SetHorizontalScrollBar(new views::OverlayScrollBar(true));
  scroller_->set_draw_overflow_indicator(false);
  AddChildView(scroller_);

  no_notifiers_view_ = new EmptyNotifierView();
  AddChildView(no_notifiers_view_);

  OnNotifierListUpdated({});
  Shell::Get()->message_center_controller()->AddNotifierSettingsListener(this);
  Shell::Get()->message_center_controller()->RequestNotifierSettingsUpdate();
}

NotifierSettingsView::~NotifierSettingsView() {
  Shell::Get()->message_center_controller()->RemoveNotifierSettingsListener(
      this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_toggle_->SetIsOn(is_quiet_mode, false /* animate */);
  if (is_quiet_mode) {
    quiet_mode_icon_->SetImage(
        gfx::CreateVectorIcon(kNotificationCenterDoNotDisturbOnIcon,
                              kMenuIconSize, kUnifiedMenuIconColor));
  } else {
    quiet_mode_icon_->SetImage(
        gfx::CreateVectorIcon(kNotificationCenterDoNotDisturbOffIcon,
                              kMenuIconSize, kUnifiedMenuIconColorDisabled));
  }
}

void NotifierSettingsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kList;
  node_data->SetName(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
}

void NotifierSettingsView::OnNotifierListUpdated(
    const std::vector<mojom::NotifierUiDataPtr>& ui_data) {
  // TODO(tetsui): currently notifier settings list doesn't update after once
  // it's loaded, in order to retain scroll position.
  if (scroller_->contents() && buttons_.size() > 0)
    return;

  buttons_.clear();

  views::View* contents_view = new ScrollContentsView();
  contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(0, kHorizontalMargin)));

  size_t notifier_count = ui_data.size();
  for (size_t i = 0; i < notifier_count; ++i) {
    NotifierButton* button = new NotifierButton(*ui_data[i], this);
    NotifierButtonWrapperView* wrapper = new NotifierButtonWrapperView(button);

    wrapper->SetFocusBehavior(FocusBehavior::ALWAYS);
    contents_view->AddChildView(wrapper);
    buttons_.insert(button);
  }

  top_label_->SetVisible(notifier_count > 0);
  no_notifiers_view_->SetVisible(notifier_count == 0);
  top_label_->InvalidateLayout();

  scroller_->SetContents(contents_view);

  contents_view->SetBoundsRect(gfx::Rect(contents_view->GetPreferredSize()));
  Layout();
}

void NotifierSettingsView::UpdateNotifierIcon(const NotifierId& notifier_id,
                                              const gfx::ImageSkia& icon) {
  for (auto* button : buttons_) {
    if (button->notifier_id() == notifier_id) {
      button->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::Layout() {
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
  if (no_notifiers_view_->visible())
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
  if (sender == title_arrow_) {
    MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
    center_view->SetSettingsVisible(!center_view->settings_visible());
    return;
  }

  if (sender == quiet_mode_toggle_) {
    MessageCenter::Get()->SetQuietMode(quiet_mode_toggle_->is_on());
    return;
  }

  auto iter = buttons_.find(static_cast<NotifierButton*>(sender));
  if (iter == buttons_.end())
    return;

  NotifierButton* button = *iter;
  button->SetChecked(!button->checked());
  Shell::Get()->message_center_controller()->SetNotifierEnabled(
      button->notifier_id(), button->checked());
  Shell::Get()->message_center_controller()->RequestNotifierSettingsUpdate();
}

}  // namespace ash
