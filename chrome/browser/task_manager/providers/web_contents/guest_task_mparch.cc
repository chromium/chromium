// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/guest_task_mparch.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/guest_page_holder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

std::u16string GetTitleForGuestMainFrame(
    content::FrameTreeNodeId main_frame_node_id) {
  auto* guest =
      guest_view::GuestViewBase::FromFrameTreeNodeId(main_frame_node_id);

  if (!guest || guest->owned_guest_page()) {
    // If we don't have a guest, or before the guest is attached. Before the
    // guest is attached, the guest's frame tree might only have an initial
    // NavigationEntry whose displayable title is empty. Return an empty title
    // for both cases.
    return u"";
  }
  // After the guest is attached.
  std::u16string guest_title =
      guest->GetController().GetLastCommittedEntry()->GetTitleForDisplay();
  base::i18n::AdjustStringForLocaleDirection(&guest_title);
  return l10n_util::GetStringFUTF16(guest->GetTaskPrefix(), guest_title);
}

}  // namespace

GuestTaskMPArch::GuestTaskMPArch(content::RenderFrameHost* guest_main_frame,
                                 base::WeakPtr<RendererTask> embedder_task)
    : RendererTask(
          /*title=*/u"",
          /*icon=*/nullptr,
          /*subframe=*/
          guest_main_frame),
      guest_main_frame_node_id_(
          guest_view::GuestViewBase::FromRenderFrameHost(guest_main_frame)
              ->guest_main_frame_tree_node_id()),
      embedder_task_(std::move(embedder_task)) {
  set_title(GetTitleForGuestMainFrame(guest_main_frame_node_id_));
}

GuestTaskMPArch::~GuestTaskMPArch() = default;

void GuestTaskMPArch::UpdateTitle() {
  set_title(GetTitleForGuestMainFrame(guest_main_frame_node_id_));
}

void GuestTaskMPArch::UpdateFavicon() {
  auto* guest =
      guest_view::GuestViewBase::FromFrameTreeNodeId(guest_main_frame_node_id_);
  if (!guest) {
    set_icon(gfx::ImageSkia());
    return;
  }
  // TODO(https://crbug.com/376084062): The favicon is currently the default
  // icon. Need to properly wire up the update.
  const content::FaviconStatus& status =
      guest->GetController().GetLastCommittedEntry()->GetFavicon();
  set_icon(status.image.IsEmpty() ? gfx::ImageSkia()
                                  : *status.image.ToImageSkia());
}

void GuestTaskMPArch::Activate() {
  embedder_task_->Activate();
}

Task::Type GuestTaskMPArch::GetType() const {
  return Task::Type::GUEST;
}

base::WeakPtr<task_manager::Task> GuestTaskMPArch::GetParentTask() const {
  return embedder_task_;
}

}  // namespace task_manager
