// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/screen_capture_tray_item_view.h"

#include <algorithm>

#include "ash/multi_capture/multi_capture_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta kMinimumTimedelta = base::Seconds(6);
}  // namespace

namespace ash {

ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata::
    ScreenCaptureTrayItemMetadata()
    : ScreenCaptureTrayItemMetadata(base::TimeTicks::Now()) {}
ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata::
    ScreenCaptureTrayItemMetadata(base::TimeTicks time_created)
    : time_created(std::move(time_created)) {}
ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata::
    ScreenCaptureTrayItemMetadata(ScreenCaptureTrayItemMetadata&& metadata) =
        default;
ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata&
ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata::operator=(
    ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata&& metadata) =
    default;
ScreenCaptureTrayItemView::ScreenCaptureTrayItemMetadata::
    ~ScreenCaptureTrayItemMetadata() = default;

ScreenCaptureTrayItemView::ScreenCaptureTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();
  UpdateLabelOrImageViewColor(/*active=*/false);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_ADMIN_SCREEN_CAPTURE));

  multi_capture_observation_.Observe(Shell::Get()->multi_capture_service());
  Refresh();
}

ScreenCaptureTrayItemView::~ScreenCaptureTrayItemView() = default;

views::View* ScreenCaptureTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return HitTestPoint(point) ? this : nullptr;
}

void ScreenCaptureTrayItemView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
      kPrivacyIndicatorsScreenShareIcon,
      active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
             : cros_tokens::kCrosSysOnSurface,
      kUnifiedTrayIconSize));
}

void ScreenCaptureTrayItemView::Refresh() {
  SetVisible(!requests_.empty());
}

void ScreenCaptureTrayItemView::MultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  // TODO(crbug.com/417490624): Remove `CHECK_DEREF` once `GetInstance()`
  // returns a reference.
  if (!base::Contains(
          CHECK_DEREF(web_app::IwaKeyDistributionInfoProvider::GetInstance())
              .GetSkipMultiCaptureNotificationBundleIds(),
          origin.host())) {
    requests_.emplace(label, ScreenCaptureTrayItemMetadata());
    Refresh();
  }
}

void ScreenCaptureTrayItemView::MultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_short_name,
    const url::Origin& app_origin) {
  MultiCaptureStarted(label, app_origin);
}

void ScreenCaptureTrayItemView::MultiCaptureStopped(const std::string& label) {
  const auto request = requests_.find(label);
  if (request == requests_.end()) {
    return;
  }

  ScreenCaptureTrayItemMetadata& metadata = request->second;
  const base::TimeDelta time_already_shown =
      base::TimeTicks::Now() - metadata.time_created;
  if (time_already_shown >= kMinimumTimedelta) {
    requests_.erase(label);
    Refresh();
  } else if (!metadata.closing_timer) {
    metadata.closing_timer = std::make_unique<base::OneShotTimer>();
    metadata.closing_timer->Start(
        FROM_HERE, kMinimumTimedelta - time_already_shown,
        base::BindOnce(&ScreenCaptureTrayItemView::MultiCaptureStopped,
                       weak_ptr_factory_.GetWeakPtr(), label));
  }
}

void ScreenCaptureTrayItemView::MultiCaptureServiceDestroyed() {
  multi_capture_observation_.Reset();
}

BEGIN_METADATA(ScreenCaptureTrayItemView)
END_METADATA

}  // namespace ash
