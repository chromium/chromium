// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/state_store_notification_observer.h"

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/state_store.h"

namespace extensions {

StateStoreNotificationObserver::StateStoreNotificationObserver(
    StateStore* state_store)
    : state_store_(state_store) {
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &StateStoreNotificationObserver::OnSessionRestoreDone,
          base::Unretained(this)));
}

StateStoreNotificationObserver::~StateStoreNotificationObserver() {
}

void StateStoreNotificationObserver::OnSessionRestoreDone(
    int /* num_tabs_restored */) {
  on_session_restored_callback_subscription_ = {};
  state_store_->RequestInitAfterDelay();
}

}  // namespace extensions
