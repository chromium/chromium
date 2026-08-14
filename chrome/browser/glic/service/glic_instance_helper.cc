// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "components/tabs/public/tab_interface.h"

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
      metrics_(std::make_unique<GlicInstanceHelperMetrics>()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
#if BUILDFLAG(IS_ANDROID)
  InitJavaObject();
#endif
}

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
    metrics_->OnBoundToInstance(bound_instance_->id());
  }
#if BUILDFLAG(IS_ANDROID)
  NotifyJavaInstanceChanged();
#endif
}

#if BUILDFLAG(IS_ANDROID)
void GlicInstanceHelper::OnInstanceChanged() {
  NotifyJavaInstanceChanged();
}
#endif

std::optional<std::string> GlicInstanceHelper::GetConversationId() const {
  if (bound_instance_) {
    return bound_instance_->conversation_id();
  }
  return std::nullopt;
}

std::string GlicInstanceHelper::GetConversationTitle() const {
  if (bound_instance_) {
    return bound_instance_->conversation_title();
  }
  return "";
}

std::optional<int> GlicInstanceHelper::GetTaskId() const {
  if (bound_instance_) {
    return bound_instance_->task_id();
  }
  return std::nullopt;
}

void GlicInstanceHelper::OnPinnedByInstance(Instance* instance) {
  CHECK(instance);
  pinned_instances_.insert(instance);
  metrics_->OnPinnedByInstance(instance->id());
  UpdateGlicPinnedToVisibleInstanceProperty();
}

void GlicInstanceHelper::OnUnpinnedByInstance(Instance* instance) {
  CHECK(instance);
  pinned_instances_.erase(instance);
  UpdateGlicPinnedToVisibleInstanceProperty();
}

std::vector<GlicInstanceHelper::Instance*>
GlicInstanceHelper::GetPinnedInstances() const {
  return std::vector<Instance*>(pinned_instances_.begin(),
                                pinned_instances_.end());
}

void GlicInstanceHelper::SetIsDaisyChained(DaisyChainSource source) {
  metrics_->SetIsDaisyChained(source);
}

void GlicInstanceHelper::OnDaisyChainAction(DaisyChainFirstAction action) {
  metrics_->OnDaisyChainAction(action);
}

base::CallbackListSubscription GlicInstanceHelper::SubscribeToDestruction(
    base::RepeatingCallback<void(tabs::TabInterface*)> callback) {
  return on_destroy_callback_list_.Add(std::move(callback));
}

void GlicInstanceHelper::OnPinnedInstanceVisibilityChanged(Instance* instance) {
  UpdateGlicPinnedToVisibleInstanceProperty();
}

void GlicInstanceHelper::UpdateGlicPinnedToVisibleInstanceProperty() {
  bool is_pinned_to_visible = false;
  for (Instance* inst : pinned_instances_) {
    if (inst->IsShowing()) {
      is_pinned_to_visible = true;
      break;
    }
  }
  if (tab_ && tab_->GetContents()) {
    GlicPerfTraitsTracker::GetInstance()
        ->NotifyIsGlicPinnedToVisibleInstanceChanged(tab_->GetContents(),
                                                     is_pinned_to_visible);
  }
}

}  // namespace glic
