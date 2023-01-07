// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_observer.h"

InfoBarObserver::InfoBarObserver(infobars::InfoBarManager* manager, Type type)
    : type_(type) {
  // There may be no |manager| if the browser window is currently closing.
  if (manager)
    infobar_observation_.Observe(manager);
}

InfoBarObserver::~InfoBarObserver() {}

void InfoBarObserver::Wait() {
  // When there is no manager being observed, there is nothing to wait on, so
  // return immediately.
  if (infobar_observation_.IsObserving())
    run_loop_.Run();
}

void InfoBarObserver::OnInfoBarAdded(infobars::InfoBar* infobar) {
  OnNotified(Type::kInfoBarAdded);
}
void InfoBarObserver::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                       bool animate) {
  OnNotified(Type::kInfoBarRemoved);
}
void InfoBarObserver::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                        infobars::InfoBar* new_infobar) {
  OnNotified(Type::kInfoBarReplaced);
}

void InfoBarObserver::OnManagerShuttingDown(infobars::InfoBarManager* manager) {
  if (run_loop_.running())
    run_loop_.Quit();
  DCHECK(infobar_observation_.IsObservingSource(manager));
  infobar_observation_.Reset();
}

void InfoBarObserver::OnNotified(Type type) {
  if (type == type_)
    run_loop_.Quit();
}
