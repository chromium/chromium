// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

namespace {
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};
}  // namespace

void TabStateStorageBackend::PopulateTabState(
    tabs_pb::TabState* tab_state,
    const TabStoragePackage& package) {
  const std::unique_ptr<AndroidTabPackage>& android_package =
      package.android_tab_package_;
  if (android_package) {
    tab_state->set_parent_id(android_package->parent_id_);
    tab_state->set_timestamp_millis(android_package->timestamp_millis_);
    if (android_package->web_contents_state_bytes_) {
      tab_state->set_web_contents_state_bytes(
          *android_package->web_contents_state_bytes_);
    }
    tab_state->set_web_contents_state_version(android_package->version_);
    if (android_package->opener_app_id_) {
      tab_state->set_opener_app_id(*android_package->opener_app_id_);
    }
    tab_state->set_theme_color(android_package->theme_color_);
    tab_state->set_launch_type_at_creation(
        android_package->launch_type_at_creation_);
    tab_state->set_last_navigation_committed_timestamp_millis(
        android_package->last_navigation_committed_timestamp_millis_);
    tab_state->set_tab_has_sensitive_content(
        android_package->tab_has_sensitive_content_);
  }
  tab_state->set_user_agent(package.user_agent_);
  tab_state->set_tab_group_id_high(package.tab_group_id_.high());
  tab_state->set_tab_group_id_low(package.tab_group_id_.low());
  tab_state->set_is_pinned(package.is_pinned_);
}

TabStateStorageBackend::TabStateStorageBackend(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)) {}

TabStateStorageBackend::~TabStateStorageBackend() {
  if (database_) {
    db_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
  }
}

void TabStateStorageBackend::Initialize() {
  database_ = std::make_unique<TabStateStorageDatabase>(profile_path_);
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::Initialize,
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnDBReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::Save(int id,
                                  int type,
                                  std::unique_ptr<TabStoragePackage> package) {
  tabs_pb::TabState tab_state;
  PopulateTabState(&tab_state, *package);
  std::string payload;
  tab_state.SerializeToString(&payload);
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::SaveNode,
                     base::Unretained(database_.get()), id, type,
                     std::move(payload), ""),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::LoadAllNodes(
    base::OnceCallback<void(std::vector<NodeState>)> callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::LoadAllNodes,
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnAllTabsRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageBackend::OnDBReady(bool success) {}

void TabStateStorageBackend::OnWrite(bool success) {}

void TabStateStorageBackend::OnAllTabsRead(
    base::OnceCallback<void(std::vector<NodeState>)> callback,
    std::vector<NodeState> result) {
  std::move(callback).Run(std::move(result));
}

}  // namespace tabs
