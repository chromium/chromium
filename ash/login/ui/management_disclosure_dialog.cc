// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/management_disclosure_dialog.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
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
#include "ui/chromeos/devicetype_utils.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Disclosure Pane.
constexpr int kPaddingDp = 32;
constexpr int kWidth = 600;
constexpr int kHeight = 640;

// Contents.
constexpr int kTitleAndInfoPaddingDp = 20;
constexpr int kLabelHeightDp = 16;

// ManagedWarningView Title.
constexpr char kManagedWarningClassName[] = "ManagedWarning";
constexpr int kSpacingBetweenEnterpriseIconAndLabelDp = 20;
constexpr int kEnterpriseIconSizeDp = 32;
constexpr int kManagedWarningViewSize =
    kSpacingBetweenEnterpriseIconAndLabelDp + kEnterpriseIconSizeDp +
    kLabelHeightDp + (2 * kPaddingDp);

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

views::Builder<views::ImageView> CreateEnterpriseIcon() {
  return views::Builder<views::ImageView>()
      .SetImage(ui::ImageModel::FromVectorIcon(
          chromeos::kEnterpriseIcon, ui::kColorIcon, kEnterpriseIconSizeDp))
      .SetHorizontalAlignment(views::ImageViewBase::Alignment::kLeading);
}

// Calculates the area of the views above the scrollable area and adds padding
// for the close button.
int GetDisclosureViewHeight() {
  return kManagedWarningViewSize + (3 * kLabelHeightDp) + (4 * kPaddingDp);
}

}  // namespace

// Container for the device monitoring warning. Composed of an optional warning
// icon on the left and a label to the right.
class ManagedWarningView : public NonAccessibleView {
  METADATA_HEADER(ManagedWarningView, NonAccessibleView)

 public:
  ManagedWarningView() : NonAccessibleView(kManagedWarningClassName) {
    const std::u16string label_text = l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_SUBTITLE_MANAGED_BY, ui::GetChromeOSDeviceName(),
        base::UTF8ToUTF16(Shell::Get()
                              ->system_tray_model()
                              ->enterprise_domain()
                              ->enterprise_domain_manager()));
    auto enterprise_image = CreateEnterpriseIcon();
    views::Builder<views::View>(this)
        .SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::LayoutOrientation::kVertical, gfx::Insets(),
            kSpacingBetweenEnterpriseIconAndLabelDp))
        .SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(kPaddingDp, kPaddingDp, 0, kPaddingDp))
        .AddChildren(
            enterprise_image,
            views::Builder<views::Label>(
                CreateLabel(label_text, views::style::STYLE_HEADLINE_5))
                .SetMultiLine(true))
        .BuildChildren();
  }

  ManagedWarningView(const ManagedWarningView&) = delete;
  ManagedWarningView& operator=(const ManagedWarningView&) = delete;

  ~ManagedWarningView() override = default;
};

BEGIN_METADATA(ManagedWarningView)
END_METADATA

ManagementDisclosureDialog::ManagementDisclosureDialog(
    const std::vector<std::u16string> disclosures,
    base::OnceClosure on_dismissed_callback) {
  SetModalType(ui::mojom::ModalType::kSystem);

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CLOSE));
  // Only have the close button.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetShowCloseButton(false);
  auto pair_cb = base::SplitOnceCallback(std::move(on_dismissed_callback));
  SetAcceptCallback(std::move(pair_cb.first));
  SetCancelCallback(std::move(pair_cb.second));
  SetShowTitle(false);

  SetPreferredSize(GetPreferredSize());

  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
  // layout_->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Set up view for the header that contains management icon and who manages
  // the device.
  AddChildView(std::make_unique<ManagedWarningView>());

  // Set up disclosure pane that will contain informational text as well as
  // disclosures.
  auto* disclosure_view = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(kPaddingDp / 2, kPaddingDp,
                                         2 * kPaddingDp, kPaddingDp))
          .Build());

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  // Create information labels.
  views::Builder<views::View>(disclosure_view)
      .AddChildren(
          views::Builder<views::Label>(
              CreateLabel(l10n_util::GetStringUTF16(
                              IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT),
                          views::style::STYLE_BODY_5))
              .SetProperty(views::kMarginsKey,
                           gfx::Insets().set_top(kTitleAndInfoPaddingDp)),
          views::Builder<views::Label>(
              CreateLabel(l10n_util::GetStringUTF16(
                              IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE),
                          views::style::STYLE_BODY_5))
              .SetProperty(views::kMarginsKey,
                           gfx::Insets().set_bottom(provider->GetDistanceMetric(
                               views::DistanceMetric::
                                   DISTANCE_UNRELATED_CONTROL_VERTICAL))),
          views::Builder<views::Label>(CreateLabel(
              l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_CONFIGURATION),
              views::style::STYLE_BODY_5)))
      .BuildChildren();

  // Set up scroll bar.
  auto* scroll_view = disclosure_view->AddChildView(
      views::Builder<views::ScrollView>()
          .SetHorizontalScrollBarMode(
              views::ScrollView::ScrollBarMode::kDisabled)
          .SetDrawOverflowIndicator(false)
          // Fill the remaining space with the list of disclosures with padding
          // on the bottom for close button.
          .ClipHeightTo(0,
                        GetPreferredSize().height() - GetDisclosureViewHeight())
          .SetBackgroundColor(std::nullopt)
          .SetAllowKeyboardScrolling(true)
          .Build());

  // Set up vertical scroll bar.
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_view->SetVerticalScrollBar(std::move(vertical_scroll));
  scroll_view->SetContents(std::make_unique<views::BulletedLabelListView>(
      disclosures, views::style::STYLE_BODY_5));

  // Parent the dialog widget to the LockSystemModalContainer to ensure that it
  // will get displayed on respective lock/signin or OOBE screen.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  int container_id = kShellWindowId_SystemModalContainer;
  if (session_controller->IsUserSessionBlocked() ||
      session_controller->GetSessionState() ==
          session_manager::SessionState::OOBE) {
    container_id = kShellWindowId_LockSystemModalContainer;
  }
  Shell::GetContainer(Shell::GetPrimaryRootWindow(), container_id);

  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(Shell::GetPrimaryRootWindow(), container_id));
  widget->Show();
}

ManagementDisclosureDialog::~ManagementDisclosureDialog() = default;

gfx::Size ManagementDisclosureDialog::GetPreferredSize() {
  return gfx::Size{kWidth, kHeight};
}

base::WeakPtr<ManagementDisclosureDialog>
ManagementDisclosureDialog::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

BEGIN_METADATA(ManagementDisclosureDialog)
END_METADATA

}  // namespace ash
