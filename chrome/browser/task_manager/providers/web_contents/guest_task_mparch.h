// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_MPARCH_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_MPARCH_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"
#include "content/public/browser/frame_tree_node_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace task_manager {

// Defines a task manager entry for an Multiple Page Architecture (MPARch) based
// GuestView.
// TODO(https://crbug.com/376084062): Remove `GuestTask` and remove the "MPArch"
// suffix after `features::kGuestViewMPArch` launches.
class GuestTaskMPArch : public RendererTask {
 public:
  GuestTaskMPArch(content::RenderFrameHost* guest_main_frame,
                  base::WeakPtr<RendererTask> parent_task);
  GuestTaskMPArch(const GuestTaskMPArch&) = delete;
  GuestTaskMPArch& operator=(const GuestTaskMPArch&) = delete;
  ~GuestTaskMPArch() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  // task_manager::Task:
  void Activate() override;
  Task::Type GetType() const override;
  base::WeakPtr<task_manager::Task> GetParentTask() const override;

 private:
  // The frame tree node ID for the inner root. Constant for the duration of a
  // GuestViewBase / GuestPageHolder.
  const content::FrameTreeNodeId guest_main_frame_node_id_;

  // Allows us to focus on the embedder's tab, and for grouping the Guest's
  // task.
  const base::WeakPtr<RendererTask> embedder_task_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_MPARCH_H_
