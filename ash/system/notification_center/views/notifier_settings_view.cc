// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notifier_settings_view.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/notification_center/message_center_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

using message_center::MessageCenter;
using message_center::NotifierId;
using ContentLayerType = AshColorProvider::ContentLayerType;

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

constexpr auto kLabelPadding = gfx::Insets::TLBR(16, 18, 15, 18);
const int kToggleButtonRowViewSpacing = 18;

constexpr auto kToggleButtonRowViewPadding = gfx::Insets::TLBR(0, 18, 0, 0);
constexpr auto kToggleButtonRowLabelPadding = gfx::Insets::TLBR(16, 0, 15, 0);
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
  METADATA_HEADER(NotifierButtonWrapperView, views::View)

 public:
  explicit NotifierButtonWrapperView(views::View* contents);

  NotifierButtonWrapperView(const NotifierButtonWrapperView&) = delete;
  NotifierButtonWrapperView& operator=(const NotifierButtonWrapperView&) =
      delete;

  ~NotifierButtonWrapperView() override;

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;

  // NotifierButton to wrap.
  raw_ptr<views::View> contents_;
};

BEGIN_METADATA(NotifierButtonWrapperView)
END_METADATA

NotifierButtonWrapperView::NotifierButtonWrapperView(views::View* contents)
    : focus_painter_(TrayPopupUtils::CreateFocusPainter()),
      contents_(contents) {
  AddChildView(contents);
}

NotifierButtonWrapperView::~NotifierButtonWrapperView() = default;

void NotifierButtonWrapperView::Layout(PassKey) {
  int contents_width = width();
  int contents_height = contents_->GetHeightForWidth(contents_width);
  int y = std::max((height() - contents_height) / 2, 0);
  contents_->SetBounds(0, y, contents_width, contents_height);

  SetFocusBehavior(contents_->GetEnabled() ? FocusBehavior::ALWAYS
                                           : FocusBehavior::NEVER);
}

gfx::Size NotifierButtonWrapperView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kWidth, kNotifierButtonWrapperHeight);
}

void NotifierButtonWrapperView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  contents_->GetViewAccessibility().GetAccessibleNodeData(node_data);
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

// ScrollContentsView ----------------------------------------------------------

class ScrollContentsView : public views::View {
  METADATA_HEADER(ScrollContentsView, views::View)

 public:
  ScrollContentsView() = default;

  ScrollContentsView(const ScrollContentsView&) = delete;
  ScrollContentsView& operator=(const ScrollContentsView&) = delete;

 private:
  void PaintChildren(const views::PaintInfo& paint_info) override {
    views::View::PaintChildren(paint_info);

    if (y() == 0) {
      return;
    }

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
};

BEGIN_METADATA(ScrollContentsView)
END_METADATA

// EmptyNotifierView -----------------------------------------------------------

class EmptyNotifierView : public views::View {
  METADATA_HEADER(EmptyNotifierView, views::View)

 public:
  EmptyNotifierView() {
    const SkColor text_color = AshColorProvider::Get()->GetContentLayerColor(
        ContentLayerType::kTextColorPrimary);
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
    label_ = AddChildView(label);
  }

  EmptyNotifierView(const EmptyNotifierView&) = delete;
  EmptyNotifierView& operator=(const EmptyNotifierView&) = delete;

 private:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        ContentLayerType::kTextColorPrimary));
  }

  raw_ptr<views::Label> label_;
};

BEGIN_METADATA(EmptyNotifierView)
END_METADATA

class NotifierViewCheckbox : public views::Checkbox {
  METADATA_HEADER(NotifierViewCheckbox, views::Checkbox)

 public:
  using views::Checkbox::Checkbox;

 private:
  // views::Checkbox:
  SkColor GetIconImageColor(int icon_state) const override {
    if (icon_state & IconState::CHECKED) {
      return AshColorProvider::Get()->GetContentLayerColor(
          ContentLayerType::kIconColorProminent);
    }
    return views::Checkbox::GetIconImageColor(icon_state);
  }
};

BEGIN_METADATA(NotifierViewCheckbox)
END_METADATA

class NotifierButtonNameView : public views::Label {
  METADATA_HEADER(NotifierButtonNameView, views::Label)

 public:
  explicit NotifierButtonNameView(const std::u16string& text)
      : views::Label(text) {}
  NotifierButtonNameView(const NotifierButtonNameView&) = delete;
  NotifierButtonNameView& operator=(const NotifierButtonNameView&) = delete;
  ~NotifierButtonNameView() override = default;

  void SetNotifierEnforced(bool enforced) {
    cached_notifier_enforced_ = enforced;
  }

 private:
  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(
        cached_notifier_enforced_
            ? SkColorSetA(GetEnabledColor(), gfx::kDisabledControlAlpha)
            : AshColorProvider::Get()->GetContentLayerColor(
                  ContentLayerType::kTextColorPrimary));
  }

  // NotifierButtonNameView uses different EnabledColor based on the notifier
  // it is associated with. Caching |cached_notifier_enforced_| allows us to use
  // the correct color for the notifier's state.
  bool cached_notifier_enforced_ = false;
};

BEGIN_METADATA(NotifierButtonNameView)
END_METADATA

// PrimaryTextColorLabel should use kTextColorPrimary instead of system colors.
class PrimaryTextColorLabel : public ::views::Label {
  METADATA_HEADER(PrimaryTextColorLabel, views::Label)

 public:
  explicit PrimaryTextColorLabel(const std::u16string& text)
      : views::Label(text) {}
  PrimaryTextColorLabel(const PrimaryTextColorLabel&) = delete;
  PrimaryTextColorLabel& operator=(const PrimaryTextColorLabel&) = delete;
  ~PrimaryTextColorLabel() override = default;

 private:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        ContentLayerType::kTextColorPrimary));
  }
};

BEGIN_METADATA(PrimaryTextColorLabel)
END_METADATA

// App badging icon should use kIconColorPrimary instead of system colors.
class AdaptiveBadgingIcon : public ::views::ImageView {
  METADATA_HEADER(AdaptiveBadgingIcon, views::ImageView)

 public:
  AdaptiveBadgingIcon() = default;
  AdaptiveBadgingIcon(const AdaptiveBadgingIcon&) = delete;
  AdaptiveBadgingIcon& operator=(const AdaptiveBadgingIcon&) = delete;
  ~AdaptiveBadgingIcon() override = default;

 private:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    SetImage(
        gfx::CreateVectorIcon(kSystemTrayAppBadgingIcon, kMenuIconSize,
                              AshColorProvider::Get()->GetContentLayerColor(
                                  ContentLayerType::kIconColorPrimary)));
  }
};

BEGIN_METADATA(AdaptiveBadgingIcon)
END_METADATA

}  // namespace

// NotifierSettingsView::NotifierButton ---------------------------------------

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
NotifierSettingsView::NotifierButton::NotifierButton(
    const NotifierMetadata& notifier)
    : views::Button(PressedCallback()), notifier_id_(notifier.notifier_id) {
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  auto icon_view = std::make_unique<views::ImageView>();
  auto name_view = std::make_unique<NotifierButtonNameView>(notifier.name);
  auto checkbox = std::make_unique<NotifierViewCheckbox>(
      std::u16string(),
      base::BindRepeating(
          [](NotifierButton* button, const ui::Event& event) {
            // The checkbox state has already changed at this point, but we'll
            // update the state on NotifierSettingsView::NotifierButtonPressed()
            // too, so here change back to the previous state.
            button->checkbox_->SetChecked(!button->checkbox_->GetChecked());
            button->NotifyClick(event);
          },
          this));
  name_view->SetAutoColorReadabilityEnabled(false);
  name_view->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary));
  name_view->SetSubpixelRenderingEnabled(false);
  // "Roboto-Regular, 13sp" is specified in the mock.
  name_view->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));

  checkbox->SetChecked(notifier.enabled);
  checkbox->SetFocusBehavior(FocusBehavior::NEVER);
  checkbox->GetViewAccessibility().SetName(notifier.name);

  if (notifier.enforced) {
    Button::SetEnabled(false);
    checkbox->SetEnabled(false);

    icon_view->SetPaintToLayer();
    icon_view->layer()->SetFillsBoundsOpaquely(false);
    icon_view->layer()->SetOpacity(gfx::kDisabledControlAlpha / float{0xFF});
    name_view->SetEnabledColor(
        SkColorSetA(name_view->GetEnabledColor(), gfx::kDisabledControlAlpha));
    name_view->SetNotifierEnforced(true);
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
    icon_view_->SetImage(
        gfx::CreateVectorIcon(message_center::kProductIcon, kEntryIconSize,
                              AshColorProvider::Get()->GetContentLayerColor(
                                  ContentLayerType::kIconColorPrimary)));
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

void NotifierSettingsView::NotifierButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  static_cast<views::View*>(checkbox_)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(node_data);
}

void NotifierSettingsView::NotifierButton::GridChanged() {
  // TODO(crbug.com/1264821): Eliminate this function, set up the layout in the
  // constructor, and replace TableLayout with BoxLayout.  Toggle the visibility
  // of the policy icon dynamically as needed.

  auto* const layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      // Add a column for the checkbox.
      ->AddPaddingColumn(views::TableLayout::kFixedSize,
                         kInnateCheckboxRightPadding)
      .AddColumn(
          views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
          views::TableLayout::kFixedSize,
          views::TableLayout::ColumnSize::kFixed, kComputedCheckboxSize, 0)

      // Add a column for the icon.
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kInternalHorizontalSpacing)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, kEntryIconSize, 0)

      // Add a column for the name.
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kSmallerInternalHorizontalSpacing)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)

      // Add a padding column which contains expandable blank space.
      .AddPaddingColumn(1.0f, 0)

      .AddRows(1, views::TableLayout::kFixedSize);

  // FocusRing is a child of Button. Ignore it.
  views::FocusRing::Get(this)->SetProperty(views::kViewIgnoredByLayoutKey,
                                           true);

  if (!GetEnabled()) {
    auto policy_enforced_icon = std::make_unique<views::ImageView>();
    policy_enforced_icon->SetImage(
        gfx::CreateVectorIcon(kSystemMenuBusinessIcon, kEntryIconSize,
                              AshColorProvider::Get()->GetContentLayerColor(
                                  ContentLayerType::kIconColorPrimary)));
    layout->AddColumn(
        views::LayoutAlignment::kCenter, views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
        kEntryIconSize, 0);
    AddChildView(std::move(policy_enforced_icon));
  }

  DeprecatedLayoutImmediately();
}

BEGIN_METADATA(NotifierSettingsView, NotifierButton)
END_METADATA

// NotifierSettingsView -------------------------------------------------------

NotifierSettingsView::NotifierSettingsView() {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto header_view = std::make_unique<views::View>();
  header_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  // There should be no bottom border under the header view if quick
  // settings notification permissions split is enabled.
  if (!features::IsSettingsAppNotificationSettingsEnabled()) {
    header_view->SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, 4, 0), kTopBorderColor));
  }

  const SkColor text_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary);
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kIconColorPrimary);

  // Row for the app badging toggle button.
  auto app_badging_icon = std::make_unique<AdaptiveBadgingIcon>();
  app_badging_icon->SetImage(gfx::CreateVectorIcon(kSystemTrayAppBadgingIcon,
                                                   kMenuIconSize, icon_color));
  auto app_badging_label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_MESSAGE_CENTER_APP_BADGING_BUTTON_TOOLTIP));
  auto app_badging_toggle =
      base::WrapUnique<views::ToggleButton>(new TrayToggleButton(
          base::BindRepeating(&NotifierSettingsView::AppBadgingTogglePressed,
                              base::Unretained(this)),
          IDS_ASH_MESSAGE_CENTER_APP_BADGING_BUTTON_TOOLTIP));
  app_badging_toggle_ = app_badging_toggle.get();

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  PrefService* prefs = session_controller->GetLastActiveUserPrefService();
  if (prefs) {
    app_badging_toggle_->SetIsOn(
        prefs->GetBoolean(prefs::kAppNotificationBadgingEnabled));
  }

  auto app_badging_view = CreateToggleButtonRow(std::move(app_badging_icon),
                                                std::move(app_badging_label),
                                                std::move(app_badging_toggle));
  app_badging_view->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 0, 1), kTopBorderColor));
  header_view->AddChildView(std::move(app_badging_view));

  // Separator between toggle button rows.
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  header_view->AddChildView(std::move(separator));

  // Row for the quiet mode toggle button.
  auto quiet_mode_icon = std::make_unique<views::ImageView>();
  quiet_mode_icon_ = quiet_mode_icon.get();
  auto quiet_mode_label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  auto quiet_mode_toggle =
      base::WrapUnique<views::ToggleButton>(new TrayToggleButton(
          base::BindRepeating(&NotifierSettingsView::QuietModeTogglePressed,
                              base::Unretained(this)),
          IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  quiet_mode_toggle_ = quiet_mode_toggle.get();
  auto quiet_mode_view = CreateToggleButtonRow(std::move(quiet_mode_icon),
                                               std::move(quiet_mode_label),
                                               std::move(quiet_mode_toggle));

  SetQuietModeState(MessageCenter::Get()->IsQuietMode());
  header_view->AddChildView(std::move(quiet_mode_view));

  // With SettingsAppNotificationSettings enabled, notification settings should
  // be managed through the settings app. The Quick Settings notification
  // settings UI should redirect users to the settings app in that case.
  // TODO(crbug/1194632): Add links to open settings page or lacros-browser.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    auto notification_settings_label =
        std::make_unique<PrimaryTextColorLabel>(l10n_util::GetStringUTF16(
            IDS_ASH_MESSAGE_CENTER_NOTIFICATION_SETTINGS_LABEL));
    notification_settings_label->SetFontList(gfx::FontList().Derive(
        kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    notification_settings_label->SetAutoColorReadabilityEnabled(false);
    notification_settings_label->SetEnabledColor(text_color);
    notification_settings_label->SetSubpixelRenderingEnabled(false);
    notification_settings_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    notification_settings_label->SetMultiLine(true);
    notification_settings_label->SetBorder(
        views::CreateEmptyBorder(kLabelPadding));
    notification_settings_label_ =
        header_view->AddChildView(std::move(notification_settings_label));
    header_view_ = AddChildView(std::move(header_view));
  } else {
    auto top_label =
        std::make_unique<PrimaryTextColorLabel>(l10n_util::GetStringUTF16(
            IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
    top_label->SetBorder(views::CreateEmptyBorder(kLabelPadding));
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
    scroller->SetBackgroundColor(std::nullopt);
    scroll_bar_ = scroller->SetVerticalScrollBar(
        std::make_unique<views::OverlayScrollBar>(
            views::ScrollBar::Orientation::kVertical));
    scroller->SetDrawOverflowIndicator(false);
    scroller_ = AddChildView(std::move(scroller));

    no_notifiers_view_ = AddChildView(std::make_unique<EmptyNotifierView>());

    OnNotifiersUpdated({});
    NotifierSettingsController::Get()->AddNotifierSettingsObserver(this);
    NotifierSettingsController::Get()->GetNotifiers();
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kList);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
}

NotifierSettingsView::~NotifierSettingsView() {
  NotifierSettingsController::Get()->RemoveNotifierSettingsObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_ && scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_toggle_->SetIsOn(is_quiet_mode);
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kIconColorPrimary);
  if (is_quiet_mode) {
    quiet_mode_icon_->SetImage(gfx::CreateVectorIcon(
        kSystemTrayDoNotDisturbIcon, kMenuIconSize, icon_color));
  } else {
    quiet_mode_icon_->SetImage(gfx::CreateVectorIcon(
        kDoNotDisturbDisabledIcon, kMenuIconSize, icon_color));
  }
}

void NotifierSettingsView::OnNotifiersUpdated(
    const std::vector<NotifierMetadata>& notifiers) {
  // We do not show notifier metadata when notifications settings are
  // split out of the notifier_settings_view.
  DCHECK(!features::IsSettingsAppNotificationSettingsEnabled());
  // TODO(tetsui): currently notifier settings list doesn't update after once
  // it's loaded, in order to retain scroll position.
  if (scroller_->contents() && buttons_.size() > 0) {
    return;
  }

  buttons_.clear();

  auto contents_view = std::make_unique<ScrollContentsView>();
  contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kHorizontalMargin)));

  for (const auto& notifier : notifiers) {
    NotifierButton* button = new NotifierButton(notifier);
    button->SetCallback(
        base::BindRepeating(&NotifierSettingsView::NotifierButtonPressed,
                            base::Unretained(this), button));
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
  DeprecatedLayoutImmediately();
}

void NotifierSettingsView::OnNotifierIconUpdated(const NotifierId& notifier_id,
                                                 const gfx::ImageSkia& icon) {
  // Notifier icons are not shown when notification permissions splitting is
  // enabled.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    return;
  }
  for (NotifierButton* button : buttons_) {
    if (button->notifier_id() == notifier_id) {
      button->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::Layout(PassKey) {
  int header_height = header_view_->GetHeightForWidth(width());
  header_view_->SetBounds(0, 0, width(), header_height);
  // |scroller_| and |no_notifiers_view_| do not exist when notifications
  // settings are split out of the notifier_settings_view.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    return;
  }

  views::View* contents_view = scroller_->contents();
  int original_scroll_position = scroller_->GetVisibleRect().y();
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
  // |scroller_| does not exist when notifications settings are split out of the
  // notifier_settings_view. Thus, minimum size should only take |header_view_|
  // into consideration.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    size.set_height(
        std::max(size.height(), header_view_->GetPreferredSize().height()));
    return size;
  }
  int total_height = header_view_->GetPreferredSize().height() +
                     scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumHeight) {
    size.Enlarge(scroller_->GetScrollBarLayoutWidth(), 0);
  }
  return size;
}

gfx::Size NotifierSettingsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size header_size = header_view_->GetPreferredSize();
  // |scroller_| and |no_notifiers_view_| do not exist when notifications
  // settings are split out of the notifier_settings_view.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    return gfx::Size(header_size.width(),
                     std::max(kMinimumHeight, header_size.height()));
  }

  gfx::Size content_size = scroller_->contents()->GetPreferredSize();
  int no_notifiers_height = 0;
  if (no_notifiers_view_->GetVisible()) {
    no_notifiers_height = no_notifiers_view_->GetPreferredSize().height();
  }
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

  // |scroller_| does not exist when notifications settings are split out of the
  // notifier_settings_view so it cannot consume key events.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    return false;
  }
  return scroller_->OnKeyPressed(event);
}

bool NotifierSettingsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // |scroller_| does not exist when notifications settings are split out of the
  // notifier_settings_view so mouse wheel events are not consumed.
  if (features::IsSettingsAppNotificationSettingsEnabled()) {
    return false;
  }
  return scroller_->OnMouseWheel(event);
}

std::unique_ptr<views::View> NotifierSettingsView::CreateToggleButtonRow(
    std::unique_ptr<views::ImageView> icon,
    std::unique_ptr<views::Label> label,
    std::unique_ptr<views::ToggleButton> toggle_button) {
  auto row_view = std::make_unique<views::View>();

  auto* row_layout =
      row_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kToggleButtonRowViewPadding, kToggleButtonRowViewSpacing));

  icon->SetBorder(views::CreateEmptyBorder(kToggleButtonRowLabelPadding));
  row_view->AddChildView(std::move(icon));

  const SkColor text_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kTextColorPrimary);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // "Roboto-Regular, 13sp" is specified in the mock.
  label->SetFontList(gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(text_color);
  label->SetSubpixelRenderingEnabled(false);
  label->SetBorder(views::CreateEmptyBorder(kToggleButtonRowLabelPadding));
  auto* label_ptr = row_view->AddChildView(std::move(label));
  row_layout->SetFlexForView(label_ptr, 1);

  toggle_button->SetFlipCanvasOnPaintForRTLUI(true);
  row_view->AddChildView(std::move(toggle_button));

  return row_view;
}

void NotifierSettingsView::AppBadgingTogglePressed() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  PrefService* prefs = session_controller->GetLastActiveUserPrefService();
  if (prefs) {
    prefs->SetBoolean(prefs::kAppNotificationBadgingEnabled,
                      app_badging_toggle_->GetIsOn());
  }
}

void NotifierSettingsView::QuietModeTogglePressed() {
  MessageCenter::Get()->SetQuietMode(quiet_mode_toggle_->GetIsOn());
}

void NotifierSettingsView::NotifierButtonPressed(NotifierButton* button) {
  button->SetChecked(!button->GetChecked());
  NotifierSettingsController::Get()->SetNotifierEnabled(button->notifier_id(),
                                                        button->GetChecked());
  NotifierSettingsController::Get()->GetNotifiers();
}

BEGIN_METADATA(NotifierSettingsView)
END_METADATA

}  // namespace ash
