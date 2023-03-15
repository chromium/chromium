// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/screen_capture_tray_item_view.h"

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace {
constexpr base::TimeDelta kMinimumTimedelta = base::Seconds(6);
}  // namespace

namespace ash {

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
  const gfx::VectorIcon* icon = &kSystemTrayRecordingIcon;
  image_view()->SetImage(gfx::CreateVectorIcon(gfx::IconDescription(
      *icon, kUnifiedTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorAlert))));

  multi_capture_service_client_observation_.Observe(
      Shell::Get()->multi_capture_service_client());
  Refresh();
}

ScreenCaptureTrayItemView::~ScreenCaptureTrayItemView() = default;

const char* ScreenCaptureTrayItemView::GetClassName() const {
  return "ScreenCaptureTrayItemView";
}

views::View* ScreenCaptureTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string ScreenCaptureTrayItemView::GetTooltipText(
    const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_ADMIN_SCREEN_CAPTURE);
}

void ScreenCaptureTrayItemView::Refresh() {
  SetVisible(!requests_.empty());
}

void ScreenCaptureTrayItemView::MultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  requests_.emplace(label,
                    ScreenCaptureTrayItemMetadata(base::TimeTicks::Now()));
  Refresh();
}

void ScreenCaptureTrayItemView::MultiCaptureStopped(const std::string& label) {
  const auto request = requests_.find(label);
  DCHECK(request != requests_.end());

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

void ScreenCaptureTrayItemView::MultiCaptureServiceClientDestroyed() {
  multi_capture_service_client_observation_.Reset();
}
}  // namespace ash
