// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/announcement_label.h"

#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ui {
namespace ime {

AnnouncementLabel::AnnouncementLabel(const std::u16string& name) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);
  GetViewAccessibility().SetName(name);
}

AnnouncementLabel::~AnnouncementLabel() = default;

void AnnouncementLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Label::GetAccessibleNodeData(node_data);
  // If there's no text to be announced, then don't make the announcement.
  if (announcement_text_.empty()) {
    return;
  }

  node_data->SetDescription(announcement_text_);
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
}

void AnnouncementLabel::AnnounceAfterDelay(const std::u16string& text,
                                           base::TimeDelta delay) {
  if (text.empty()) {
    return;
  }
  delay_timer_ = std::make_unique<base::OneShotTimer>();
  delay_timer_->Start(FROM_HERE, delay,
                      base::BindOnce(&AnnouncementLabel::DoAnnouncement,
                                     base::Unretained(this), text));
}

void AnnouncementLabel::DoAnnouncement(const std::u16string text) {
  announcement_text_ = text;

  GetViewAccessibility().SetDescription(announcement_text_);

  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged,
                           /*send_native_event=*/false);
}

BEGIN_METADATA(AnnouncementLabel)
END_METADATA

}  // namespace ime
}  // namespace ui
