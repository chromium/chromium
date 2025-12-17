// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

namespace glic {

DEFINE_USER_DATA(GlicInstanceHelper);

GlicInstanceHelper* GlicInstanceHelper::From(tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

GlicInstanceHelper::GlicInstanceHelper(tabs::TabInterface* tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

GlicInstanceHelper::~GlicInstanceHelper() {
  CHECK(tab_);
  on_destroy_callback_list_.Notify(tab_);
}

void GlicInstanceHelper::SetInstanceId(const InstanceId& instance_id) {
  instance_id_ = instance_id;
  metrics_.OnBoundToInstance(instance_id);
}

void GlicInstanceHelper::OnPinnedByInstance(const InstanceId& instance_id) {
  metrics_.OnPinnedByInstance(instance_id);
}

void GlicInstanceHelper::SetIsDaisyChained() {
  metrics_.SetIsDaisyChained();
}

void GlicInstanceHelper::OnDaisyChainAction(DaisyChainFirstAction action) {
  metrics_.OnDaisyChainAction(action);
}

base::CallbackListSubscription GlicInstanceHelper::SubscribeToDestruction(
    base::RepeatingCallback<void(tabs::TabInterface*)> callback) {
  return on_destroy_callback_list_.Add(std::move(callback));
}

}  // namespace glic
