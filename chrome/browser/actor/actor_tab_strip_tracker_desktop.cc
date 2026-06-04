// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_tab_strip_tracker_desktop.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace actor {

ActorTabStripTrackerDesktop::ActorTabStripTrackerDesktop(
    ActorKeyedService& service,
    bool enable_tab_tracking)
    : service_(service) {
  if (enable_tab_tracking) {
    tab_strip_tracker_ = std::make_unique<BrowserTabStripTracker>(this, this);
    tab_strip_tracker_->Init();
  }
}

ActorTabStripTrackerDesktop::~ActorTabStripTrackerDesktop() = default;

bool ActorTabStripTrackerDesktop::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser->GetProfile() == service_->GetProfile();
}

void ActorTabStripTrackerDesktop::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kRemoved) {
    const TabStripModelChange::Remove* remove = change.GetRemove();
    for (const auto& removed_tab : remove->contents) {
      if (TabRemoveReasonUtils::WillDeleteWebContents(
              removed_tab.remove_reason)) {
        tabs::TabHandle handle = removed_tab.tab->GetHandle();
        std::vector<TaskId> tasks_to_notify =
            service_->FindTaskIdsInActive([&handle](const ActorTask& task) {
              return task.HasTab(handle) ||
                     task.GetLastActedTabs().contains(handle);
            });

        for (TaskId id : tasks_to_notify) {
          ActorTask* task = service_->GetTask(id);
          if (task) {
            task->OnTabWillDetach(handle);
          }
        }
      }
    }
  }
}

}  // namespace actor
