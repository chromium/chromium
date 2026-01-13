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

std::optional<InstanceId> GlicInstanceHelper::GetInstanceId() const {
  if (bound_instance_) {
    return bound_instance_->id();
  }
  return std::nullopt;
}

void GlicInstanceHelper::SetBoundInstance(Instance* instance) {
  bound_instance_ = instance;
  if (bound_instance_) {
    metrics_.OnBoundToInstance(bound_instance_->id());
  }
}

std::optional<std::string> GlicInstanceHelper::GetConversationId() const {
  if (bound_instance_) {
    return bound_instance_->conversation_id();
  }
  return std::nullopt;
}

void GlicInstanceHelper::OnPinnedByInstance(Instance* instance) {
  CHECK(instance);
  pinned_instances_.insert(instance);
  metrics_.OnPinnedByInstance(instance->id());
}

void GlicInstanceHelper::OnUnpinnedByInstance(Instance* instance) {
  CHECK(instance);
  pinned_instances_.erase(instance);
}

std::vector<GlicInstanceHelper::Instance*>
GlicInstanceHelper::GetPinnedInstances() const {
  return std::vector<Instance*>(pinned_instances_.begin(),
                                pinned_instances_.end());
}

void GlicInstanceHelper::SetIsDaisyChained(DaisyChainSource source) {
  metrics_.SetIsDaisyChained(source);
}

void GlicInstanceHelper::OnDaisyChainAction(DaisyChainFirstAction action) {
  metrics_.OnDaisyChainAction(action);
}

base::CallbackListSubscription GlicInstanceHelper::SubscribeToDestruction(
    base::RepeatingCallback<void(tabs::TabInterface*)> callback) {
  return on_destroy_callback_list_.Add(std::move(callback));
}

}  // namespace glic
