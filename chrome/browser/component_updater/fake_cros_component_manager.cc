// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/fake_cros_component_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace component_updater {

FakeCrOSComponentManager::ComponentInfo::ComponentInfo(
    Error load_response,
    const base::FilePath& install_path,
    const base::FilePath& mount_path)
    : load_response(load_response),
      install_path(install_path),
      mount_path(mount_path) {
  // If component load fails, neither install nor mount path should be set.
  DCHECK(load_response == Error::NONE ||
         (install_path.empty() && mount_path.empty()));
  // Component should have install path set if it's expected to be loaded.
  DCHECK(load_response != Error::NONE || !install_path.empty());
}

FakeCrOSComponentManager::ComponentInfo::~ComponentInfo() = default;

FakeCrOSComponentManager::FakeCrOSComponentManager() = default;

FakeCrOSComponentManager::~FakeCrOSComponentManager() = default;

bool FakeCrOSComponentManager::FinishLoadRequest(
    const std::string& name,
    const ComponentInfo& component_info) {
  if (!pending_loads_.count(name) || pending_loads_[name].empty()) {
    LOG(ERROR) << "No pending load for " << name;
    return false;
  }

  auto& pending_load = pending_loads_[name].front();
  FinishComponentLoad(name, pending_load.mount_requested, component_info);

  LoadCallback callback = std::move(pending_load.callback);
  pending_loads_[name].pop_front();

  std::move(callback).Run(component_info.load_response,
                          component_info.load_response == Error::NONE
                              ? component_info.mount_path
                              : base::FilePath());
  return true;
}

bool FakeCrOSComponentManager::ResetComponentState(const std::string& name,
                                                   const ComponentInfo& state) {
  if (!supported_components_.count(name))
    return false;

  installed_components_.erase(name);
  mounted_components_.erase(name);

  component_infos_.erase(name);
  component_infos_.emplace(
      name,
      ComponentInfo(state.load_response, state.install_path, state.mount_path));
  return true;
}

bool FakeCrOSComponentManager::HasPendingInstall(
    const std::string& name) const {
  DCHECK(queue_load_requests_);

  const auto& it = pending_loads_.find(name);
  return it != pending_loads_.end() && !it->second.empty();
}

bool FakeCrOSComponentManager::UpdateRequested(const std::string& name) const {
  DCHECK(queue_load_requests_);

  const auto& it = pending_loads_.find(name);
  return it != pending_loads_.end() && !it->second.empty() &&
         it->second.front().needs_update;
}

void FakeCrOSComponentManager::SetDelegate(Delegate* delegate) {
  // No-op, not used by the fake.
}

void FakeCrOSComponentManager::Load(const std::string& name,
                                    MountPolicy mount_policy,
                                    UpdatePolicy update_policy,
                                    LoadCallback load_callback) {
  if (!supported_components_.count(name)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  Error::UNKNOWN_COMPONENT, base::FilePath()));
    return;
  }

  bool needs_update = update_policy == UpdatePolicy::kForce ||
                      (!installed_components_.count(name) &&
                       update_policy != UpdatePolicy::kSkip);

  // The request has to be handled if the component is not yet installed, or it
  // requires immediate update.
  if (needs_update || !installed_components_.count(name)) {
    HandlePendingRequest(name, mount_policy == MountPolicy::kMount,
                         needs_update, std::move(load_callback));
    return;
  }

  // Handle request if the component has yet to be mounted - e.g. if previous
  // loads installed the component without mounting it.
  if (!mounted_components_.count(name) && mount_policy == MountPolicy::kMount) {
    HandlePendingRequest(name, true /*mount_requested*/, false /*needs_update*/,
                         std::move(load_callback));
    return;
  }

  // The component has been prevoiusly installed, and mounted as required by
  // this load request - run the callback according to the existing state.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(load_callback), Error::NONE,
                                mount_policy == MountPolicy::kMount
                                    ? mounted_components_[name]
                                    : base::FilePath()));
}

bool FakeCrOSComponentManager::Unload(const std::string& name) {
  registered_components_.erase(name);
  mounted_components_.erase(name);
  installed_components_.erase(name);
  return true;
}

void FakeCrOSComponentManager::RegisterCompatiblePath(
    const std::string& name,
    const base::FilePath& path) {
  installed_components_[name] = path;
}

void FakeCrOSComponentManager::UnregisterCompatiblePath(
    const std::string& name) {
  installed_components_.erase(name);
}

base::FilePath FakeCrOSComponentManager::GetCompatiblePath(
    const std::string& name) const {
  const auto& it = installed_components_.find(name);
  if (it == installed_components_.end())
    return base::FilePath();
  return it->second;
}

bool FakeCrOSComponentManager::IsRegistered(const std::string& name) const {
  return registered_components_.count(name);
}

void FakeCrOSComponentManager::RegisterInstalled() {
  NOTIMPLEMENTED();
}

FakeCrOSComponentManager::LoadRequest::LoadRequest(bool mount_requested,
                                                   bool needs_update,
                                                   LoadCallback callback)
    : mount_requested(mount_requested),
      needs_update(needs_update),
      callback(std::move(callback)) {}

FakeCrOSComponentManager::LoadRequest::~LoadRequest() = default;

void FakeCrOSComponentManager::HandlePendingRequest(const std::string& name,
                                                    bool mount_requested,
                                                    bool needs_update,
                                                    LoadCallback callback) {
  if (queue_load_requests_) {
    pending_loads_[name].emplace_back(mount_requested, needs_update,
                                      std::move(callback));
    return;
  }

  const auto& component_info = component_infos_.find(name);
  if (component_info == component_infos_.end()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Error::INSTALL_FAILURE,
                                  base::FilePath()));
    return;
  }

  FinishComponentLoad(name, mount_requested, component_info->second);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), component_info->second.load_response,
                     component_info->second.mount_path));
}

void FakeCrOSComponentManager::FinishComponentLoad(
    const std::string& name,
    bool mount_requested,
    const ComponentInfo& component_info) {
  registered_components_.insert(name);
  if (component_info.load_response != Error::NONE)
    return;

  DCHECK_EQ(mount_requested, !component_info.mount_path.empty());
  installed_components_[name] = component_info.install_path;
  if (mount_requested)
    mounted_components_[name] = component_info.mount_path;
}

}  // namespace component_updater
