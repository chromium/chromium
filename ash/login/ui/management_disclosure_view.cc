// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/management_disclosure_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr const char kManagementDisclosureViewClassName[] =
    "ManagementDisclosureView";

// Landscape Pane.
constexpr int kTopPaddingDp = 100;
constexpr int kShelfPaddingDp = 100;
constexpr int kDefaultShelfHeightDp = 48;
constexpr int kPaddingDp = 32;

// ManagedWarningView Title.
constexpr char kManagedWarningClassName[] = "ManagedWarning";
constexpr int kSpacingBetweenEnterpriseIconAndLabelDp = 20;
constexpr int kEnterpriseIconSizeDp = 32;

// Contents.
constexpr int kTitleAndInfoPaddingDp = 20;

// Bullet.
constexpr int kBulletLabelPaddingDp = 3;
constexpr int kBulletRadiusDp = 3;
constexpr int kBulletContainerSizeDp = 30;

std::unique_ptr<views::Label> CreateLabel(const std::u16string& text,
                                          views::style::TextStyle style) {
  auto label = std::make_unique<views::Label>(text);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetTextStyle(style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColorId(
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshTextColorPrimary);
  label->SetMultiLine(true);
  return label;
}

class ManagementDisclosureEventHandler : public ui::EventHandler {
 public:
  explicit ManagementDisclosureEventHandler(ManagementDisclosureView* view)
      : view_(view) {
    // Used so that when a user clicks area outside disclosure window it closes.
    Shell::Get()->AddPreTargetHandler(this);
  }

  ManagementDisclosureEventHandler(const ManagementDisclosureEventHandler&) =
      delete;
  ManagementDisclosureEventHandler& operator=(
      const ManagementDisclosureEventHandler&) = delete;

  ~ManagementDisclosureEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if ((event->type() == ui::EventType::kGestureTap ||
         event->type() == ui::EventType::kGestureTapDown)) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnKeyEvent(ui::KeyEvent* event) override { view_->OnKeyEvent(event); }

  raw_ptr<ManagementDisclosureView> view_;
};

class BulletView : public views::View {
  METADATA_HEADER(BulletView, views::View)

 public:
  explicit BulletView(SkColor color, int radius)
      : color_(color), radius_(radius) {}
  BulletView(const BulletView&) = delete;
  BulletView& operator=(const BulletView&) = delete;
  ~BulletView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);

    SkPath path;
    path.addCircle(GetLocalBounds().CenterPoint().x(),
                   GetLocalBounds().CenterPoint().y(), radius_);
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color_);
    flags.setAntiAlias(true);

    canvas->DrawPath(path, flags);
  }

 private:
  SkColor color_;
  int radius_;
};

BEGIN_METADATA(BulletView)
END_METADATA

}  // namespace

// Container for the device monitoring warning. Composed of an optional warning
// icon on the left and a label to the right.
class ManagedWarningView : public NonAccessibleView {
  METADATA_HEADER(ManagedWarningView, NonAccessibleView)

 public:
  ManagedWarningView() : NonAccessibleView(kManagedWarningClassName) {
    // TODO(b/330527825): Replace with string.
    const std::u16string label_text = u"Your chromebook is managed by ";
    // base::UTF8ToUTF16(device_manager_.value());

    views::LayoutProvider* provider = views::LayoutProvider::Get();

    views::Builder<views::View>(this)
        .SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::LayoutOrientation::kVertical, gfx::Insets(),
            kSpacingBetweenEnterpriseIconAndLabelDp))
        .AddChildren(
            views::Builder<views::ImageView>().CopyAddressTo(&image_).SetImage(
                ui::ImageModel::FromVectorIcon(chromeos::kEnterpriseIcon,
                                               kEnterpriseIconSizeDp)),
            views::Builder<views::View>()
                .CopyAddressTo(&placeholder_)
                .SetVisible(true)
                .SetPreferredSize(gfx::Size(0, kEnterpriseIconSizeDp)),
            views::Builder<views::Label>(
                CreateLabel(label_text, views::style::STYLE_HEADLINE_5))
                .SetMultiLine(true)
                .CopyAddressTo(&label_)
                .SetLineHeight(provider->GetDistanceMetric(
                    views::DistanceMetric::DISTANCE_UNRELATED_CONTROL_VERTICAL))
                .SetProperty(
                    views::kMarginsKey,
                    gfx::Insets(provider->GetDistanceMetric(
                        views::DistanceMetric::
                            DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL))))
        .BuildChildren();
  }

  ManagedWarningView(const ManagedWarningView&) = delete;
  ManagedWarningView& operator=(const ManagedWarningView&) = delete;

  ~ManagedWarningView() override = default;

 private:
  std::optional<std::string> device_manager_;
  raw_ptr<views::ImageView> image_;
  raw_ptr<views::Label> label_;
  raw_ptr<views::View> placeholder_;
};

BEGIN_METADATA(ManagedWarningView)
END_METADATA

ManagementDisclosureView::ManagementDisclosureView(
    const OnManagementDisclosureDismissed& on_dismissed)
    : NonAccessibleView(kManagementDisclosureViewClassName),
      on_dismissed_(on_dismissed),
      event_handler_(std::make_unique<ManagementDisclosureEventHandler>(this)) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  if (chromeos::features::IsJellyrollEnabled()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated,
        provider->GetCornerRadiusMetric(
            views::ShapeContextTokens::kSidePanelContentRadius)));
    SetBorder(std::make_unique<views::HighlightBorder>(
        provider->GetCornerRadiusMetric(
            views::ShapeContextTokens::kSidePanelContentRadius),
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
    shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
        this, SystemShadow::Type::kElevation12);
    shadow_->SetRoundedCornerRadius(provider->GetCornerRadiusMetric(
        views::ShapeContextTokens::kSidePanelContentRadius));
  } else {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase80,
        provider->GetCornerRadiusMetric(
            views::ShapeContextTokens::kBadgeRadius)));
  }

  SetPreferredSize(GetPreferredSizeLandscape());

  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Set up view for the header that contains management icon and who manages
  // the device.
  managed_warning_view_ = AddChildView(std::make_unique<ManagedWarningView>());

  // Set up disclosure pane that will contain informational text as well as
  // disclosures.
  disclosure_view_ =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .Build());

  // Create information labels.
  views::Builder<views::View>(disclosure_view_.get())
      .AddChildren(
          views::Builder<views::Label>(
              CreateLabel(l10n_util::GetStringUTF16(
                              IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT),
                          views::style::STYLE_BODY_5))
              .CopyAddressTo(&admin_description_label_)
              .SetProperty(
                  views::kMarginsKey,
                  gfx::Insets().set_top(kTitleAndInfoPaddingDp).set_bottom(0)),
          views::Builder<views::Label>(
              CreateLabel(l10n_util::GetStringUTF16(
                              IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE),
                          views::style::STYLE_BODY_5))
              .CopyAddressTo(&additional_information_label_)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets().set_top(0).set_bottom(
                               provider->GetDistanceMetric(
                                   views::DistanceMetric::
                                       DISTANCE_UNRELATED_CONTROL_VERTICAL))),
          views::Builder<views::Label>(

              CreateLabel(l10n_util::GetStringUTF16(
                              IDS_MANAGEMENT_DEVICE_CONFIGURATION),
                          views::style::STYLE_BODY_5))
              .CopyAddressTo(&may_be_able_to_view_title_))
      .BuildChildren();

  // Set up scroll bar.
  scroll_view_ = disclosure_view_->AddChildView(
      views::Builder<views::ScrollView>()
          .SetHorizontalScrollBarMode(
              views::ScrollView::ScrollBarMode::kDisabled)
          .SetDrawOverflowIndicator(false)
          .ClipHeightTo(0, std::numeric_limits<int>::max())
          .SetBackgroundColor(std::nullopt)
          .SetAllowKeyboardScrolling(true)
          .Build());

  // Set up vertical scroll bar.
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  // Set up scroll contents.
  auto scroll_contents = std::make_unique<views::View>();
  auto* layout =
      scroll_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  // Create bulleted list.
  auto add_bulleted_label = [&](const std::u16string& text) {
    auto* container = new views::View();
    auto layout = std::make_unique<views::FlexLayout>();
    // Align the bullet to the top line of multi-line labels.
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    container->SetLayoutManager(std::move(layout));
    auto label =
        views::Builder<views::Label>(
            CreateLabel(text, views::style::STYLE_BODY_5))
            .SetMaximumWidth(disclosure_view_->width() -
                             kBulletContainerSizeDp - kPaddingDp)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(kBulletLabelPaddingDp, kBulletLabelPaddingDp,
                                  kBulletLabelPaddingDp, kPaddingDp))
            .Build();

    BulletView* bullet_view =
        new BulletView(kColorAshTextColorPrimary, kBulletRadiusDp);
    bullet_view->SetPreferredSize(
        gfx::Size(kBulletContainerSizeDp, kBulletContainerSizeDp));

    container->AddChildView(bullet_view);
    container->AddChildView(label.get());
    scroll_contents->AddChildView(container);
  };

  // These are just placeholder disclosures.
  // Add bulleted list of device disclosures.
  add_bulleted_label(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES));
  add_bulleted_label(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_DATA));
  add_bulleted_label(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_REPORT_APP_INFO_AND_ACTIVITY));
  //add_bulleted_label(
  //    l10n_util::GetStringUTF16(IDS_MANAGEMENT_LOG_UPLOAD_ENABLED_NO_LINK));
  //add_bulleted_label(
     // l10n_util::GetStringUTF16(IDS_MANAGEMENT_LEGACY_TECH_REPORT_NO_LINK));

  scroll_view_->SetContents(std::move(scroll_contents));

  // Close button.
  close_button_ = AddChildView(
      views::Builder<PillButton>()
          .SetCallback(base::BindRepeating(&ManagementDisclosureView::Hide,
                                           base::Unretained(this)))
          .SetText(l10n_util::GetStringUTF16(IDS_CLOSE))
          .SetProperty(views::kViewIgnoredByLayoutKey, true)
          .Build());
  if (chromeos::features::IsJellyrollEnabled()) {
    close_button_->SetBackgroundColorId(
        cros_tokens::kCrosSysSystemPrimaryContainer);
  }
}

ManagementDisclosureView::~ManagementDisclosureView() = default;

void ManagementDisclosureView::ProcessPressedEvent(
    const ui::LocatedEvent* event) {
  if (!GetVisible()) {
    return;
  }

  if (GetBoundsInScreen().Contains(event->root_location())) {
    return;
  }

  Hide();
}

void ManagementDisclosureView::Hide() {
  shadow_.reset();
  SetVisible(false);
  on_dismissed_.Run();
}

gfx::Size ManagementDisclosureView::GetPreferredSizeLandscape() {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  bounds.Inset(gfx::Insets::TLBR(kTopPaddingDp, bounds.width() / 4,
                                 ShelfConfig::Get()
                                     ? ShelfConfig::Get()->shelf_size()
                                     : kDefaultShelfHeightDp + kShelfPaddingDp,
                                 bounds.width() / 4));
  return bounds.size();
}
gfx::Size ManagementDisclosureView::GetPreferredSizePortrait() {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  // Want to keep the width the same between landscape and portrait.
  auto landscape_width = bounds.height() / 2;
  int width_insets;
  if (landscape_width < bounds.width() - (kPaddingDp * 2)) {
    width_insets = bounds.height() / 4;
  } else {
    width_insets = kPaddingDp;
  }
  bounds.Inset(gfx::Insets::TLBR(kTopPaddingDp, width_insets,
                                 ShelfConfig::Get()
                                     ? ShelfConfig::Get()->shelf_size()
                                     : kDefaultShelfHeightDp + kShelfPaddingDp,
                                 width_insets));
  return bounds.size();
}

void ManagementDisclosureView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (bounds().width() >= bounds().height()) {
    UseLandscapeLayout();
  } else {
    UsePortraitLayout();
  }
}

void ManagementDisclosureView::Layout(PassKey) {
  LayoutSuperclass<View>(this);

  close_button_->SizeToPreferredSize();
  const int submit_button_x =
      size().width() - kPaddingDp - close_button_->size().width();
  const int submit_button_y =
      size().height() - kPaddingDp - close_button_->size().height();
  close_button_->SetPosition(gfx::Point{submit_button_x, submit_button_y});
}

void ManagementDisclosureView::OnKeyEvent(ui::KeyEvent* event) {
  if (!GetVisible() || event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    Hide();
  }
}

void ManagementDisclosureView::UseLandscapeLayout() {
  disclosure_view_->SetPreferredSize(GetPreferredSizeLandscape());
  disclosure_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPaddingDp / 2, kPaddingDp,
                        close_button_->height() + kPaddingDp, kPaddingDp));
}

void ManagementDisclosureView::UsePortraitLayout() {
  disclosure_view_->SetPreferredSize(GetPreferredSizePortrait());
  disclosure_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPaddingDp / 2, kPaddingDp,
                        close_button_->height() + kPaddingDp, kPaddingDp));
}

BEGIN_METADATA(ManagementDisclosureView)
END_METADATA

}  // namespace ash
