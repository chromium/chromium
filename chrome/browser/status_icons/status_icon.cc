// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon.h"

#include <utility>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/status_icons/status_icon_observer.h"

StatusIcon::StatusIcon() {
}

StatusIcon::~StatusIcon() {
}

void StatusIcon::AddObserver(StatusIconObserver* observer) {
  observers_.AddObserver(observer);
}

void StatusIcon::RemoveObserver(StatusIconObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool StatusIcon::HasObservers() const {
  return !observers_.empty();
}

void StatusIcon::DispatchClickEvent() {
  for (StatusIconObserver& observer : observers_)
    observer.OnStatusIconClicked();
}

#if BUILDFLAG(IS_WIN)
void StatusIcon::DispatchBalloonClickEvent() {
  for (StatusIconObserver& observer : observers_)
    observer.OnBalloonClicked();
}
#endif

void StatusIcon::ForceVisible() {}

void StatusIcon::SetContextMenu(std::unique_ptr<StatusIconMenuModel> menu) {
  // The UI may been showing a menu for the current model, don't destroy it
  // until we've notified the UI of the change.
  std::unique_ptr<StatusIconMenuModel> old_menu =
      std::move(context_menu_contents_);
  context_menu_contents_ = std::move(menu);
  UpdatePlatformContextMenu(context_menu_contents_.get());
}
