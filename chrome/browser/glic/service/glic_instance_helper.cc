// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

namespace glic {

DEFINE_USER_DATA(GlicInstanceHelper);

GlicInstanceHelper* GlicInstanceHelper::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

GlicInstanceHelper::GlicInstanceHelper(tabs::TabInterface* tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

GlicInstanceHelper::~GlicInstanceHelper() {
  if (!instance_id_.has_value()) {
    return;
  }
  CHECK(tab_);
  on_destroy_callback_list_.Notify(tab_, instance_id_.value());
}

base::CallbackListSubscription GlicInstanceHelper::SubscribeToDestruction(
    base::RepeatingCallback<void(tabs::TabInterface*, const InstanceId&)>
        callback) {
  return on_destroy_callback_list_.Add(std::move(callback));
}

}  // namespace glic
