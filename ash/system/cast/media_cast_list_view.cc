// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/media_cast_list_view.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/global_media_controls/public/mojom/device_service.mojom-shared.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kDeviceIdKey, nullptr)

// Extra spacing to add between cast stop buttons and the edge of `tri_view()`
// header entry.
constexpr gfx::Insets kItemTriViewPaddings =
    gfx::Insets::TLBR(0, 0, 0, kTrayPopupLabelRightPadding);

// The paddings for the header entry and cast item entries.
constexpr gfx::Insets kHeaderInsets = gfx::Insets::TLBR(0, 0, 0, 8);
constexpr gfx::Insets kHighlightHoverViewInsets = gfx::Insets::VH(0, 16);

// Returns the correct vector icon for the icon type.
// TODO(b/327507429): Revisit the icons for each casting type.
const gfx::VectorIcon& GetVectorIcon(
    global_media_controls::mojom::IconType icon) {
  switch (icon) {
    case global_media_controls::mojom::IconType::kInfo:
      return kSystemMenuCastGenericIcon;
    case global_media_controls::mojom::IconType::kInput:
      return kSystemMenuCastGenericIcon;
    case global_media_controls::mojom::IconType::kSpeaker:
      return kSystemMenuCastAudioIcon;
    case global_media_controls::mojom::IconType::kSpeakerGroup:
      return kSystemMenuCastAudioGroupIcon;
    case global_media_controls::mojom::IconType::kTv:
      return kSystemMenuCastGenericIcon;
      // In these cases the icon is a placeholder and doesn't actually get
      // shown.
      // TODO(b/327507429): Show the loading throbber in the
      // `MediaCastListView`.
    case global_media_controls::mojom::IconType::kThrobber:
    case global_media_controls::mojom::IconType::kUnknown:
      return kSystemMenuCastGenericIcon;
  }
}

}  // namespace

MediaCastListView::MediaCastListView(
    base::RepeatingClosure stop_casting_callback,
    base::RepeatingCallback<void(const std::string& device_id)>
        start_casting_callback,
    base::RepeatingCallback<void(const bool has_devices)>
        on_devices_updated_callback,
    mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
        receiver)
    : TrayDetailedView(/*delegate=*/nullptr),
      on_stop_casting_callback_(std::move(stop_casting_callback)),
      on_start_casting_callback_(std::move(start_casting_callback)),
      on_devices_updated_callback_(std::move(on_devices_updated_callback)),
      receiver_(this, std::move(receiver)) {
  // Creates the cast item container.
  item_container_ =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .Build());
}

MediaCastListView::~MediaCastListView() = default;

void MediaCastListView::OnDevicesUpdated(
    std::vector<global_media_controls::mojom::DevicePtr> devices) {
  // Update UI.
  item_container_->RemoveAllChildViews();

  if (!devices.empty()) {
    CreateCastingHeader();
  }

  // Add a view for each receiver.
  for (const auto& device : devices) {
    HoverHighlightView* container =
        AddScrollListItem(item_container_, GetVectorIcon(device->icon),
                          base::UTF8ToUTF16(device->name));
    container->tri_view()->SetInsets(kHighlightHoverViewInsets);
    container->SetProperty(kDeviceIdKey, device->id);
  }

  // Inform the Panel view on devices update.
  on_devices_updated_callback_.Run(
      /*has_devices=*/!devices.empty());

  item_container_->InvalidateLayout();
}

void MediaCastListView::HandleViewClicked(views::View* view) {
  if (view->GetProperty(kDeviceIdKey)) {
    on_start_casting_callback_.Run(*view->GetProperty(kDeviceIdKey));
  }
}

// TODO(b/327507429): Refactor this method to share it with the
// CastDetailedView.
std::unique_ptr<PillButton> MediaCastListView::CreateStopButton() {
  auto stop_button = std::make_unique<PillButton>(
      base::BindRepeating(
          [](base::WeakPtr<MediaCastListView> view) {
            view->on_stop_casting_callback_.Run();
          },
          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP_CASTING),
      PillButton::kDefaultWithIconLeading, &kQuickSettingsCircleStopIcon);
  stop_button->SetBackgroundColorId(cros_tokens::kCrosSysErrorContainer);
  stop_button->SetIconColorId(cros_tokens::kCrosSysError);
  stop_button->SetButtonTextColorId(cros_tokens::kCrosSysError);
  stop_button->SetID(kStopCastingButtonId);
  return stop_button;
}

void MediaCastListView::CreateCastingHeader() {
  auto casting_header = base::WrapUnique(TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/false));
  TrayPopupUtils::ConfigureHeader(casting_header.get());

  // Set casting icon on left side.
  auto image_view = base::WrapUnique(
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false));
  image_view->SetImage(gfx::CreateVectorIcon(
      on_stop_casting_callback_.is_null() ? kQuickSettingsCastIcon
                                          : kQuickSettingsCastConnectedIcon,
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface)));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  casting_header->AddView(TriView::Container::START, image_view.release());

  // Set message label in middle of row.
  std::unique_ptr<views::Label> label =
      base::WrapUnique(TrayPopupUtils::CreateDefaultLabel());
  label->SetText(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_CAST_LIST_HEADER));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  label->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label);
  casting_header->AddView(TriView::Container::CENTER, std::move(label));
  casting_header->SetContainerBorder(
      TriView::Container::CENTER,
      views::CreateEmptyBorder(kItemTriViewPaddings));

  // Add stop button to the entry if it's casting.
  if (!on_stop_casting_callback_.is_null()) {
    std::unique_ptr<PillButton> stop_button = CreateStopButton();
    casting_header->AddView(TriView::Container::END, stop_button.release());
    casting_header->SetContainerVisible(TriView::Container::END, true);
  } else {
    // Nothing to the right of the text.
    casting_header->SetContainerVisible(TriView::Container::END, false);
  }
  casting_header->SetInsets(kHeaderInsets);
  item_container_->AddChildView(std::move(casting_header));
}

BEGIN_METADATA(MediaCastListView)
END_METADATA

}  // namespace ash
