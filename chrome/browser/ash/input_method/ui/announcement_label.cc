// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/announcement_label.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ui {
namespace ime {

constexpr base::TimeDelta kAnnouncementDelayMs = base::Milliseconds(100);

AnnouncementLabel::AnnouncementLabel() = default;

AnnouncementLabel::~AnnouncementLabel() = default;

void AnnouncementLabel::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kImeCandidate;
  node_data->SetName(GetAccessibleName());
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
}

void AnnouncementLabel::Announce(const std::u16string& text) {
  if (text.empty())
    return;
  SetAccessibleName(text);
  delay_timer_ = std::make_unique<base::OneShotTimer>();
  delay_timer_->Start(
      FROM_HERE, kAnnouncementDelayMs,
      base::BindOnce(&AnnouncementLabel::DoAnnouncement,
                     base::Unretained(this)));
}

void AnnouncementLabel::DoAnnouncement() {
  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged,
                           /*send_native_event=*/true);
}

BEGIN_METADATA(AnnouncementLabel)
END_METADATA

}  // namespace ime
}  // namespace ui
