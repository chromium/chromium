// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_model.h"

namespace ash {

RemoteAppsModel::AppInfo::AppInfo(const std::string& id,
                                  const std::string& name,
                                  const GURL& icon_url,
                                  std::string folder_id,
                                  bool add_to_front)
    : id(id),
      name(name),
      icon_url(icon_url),
      folder_id(folder_id),
      icon(gfx::ImageSkia()),
      add_to_front(add_to_front) {}

RemoteAppsModel::AppInfo::AppInfo(const AppInfo& other) = default;

RemoteAppsModel::AppInfo::~AppInfo() = default;

RemoteAppsModel::FolderInfo::FolderInfo(const std::string& id,
                                        const std::string& name,
                                        bool add_to_front)
    : id(id), name(name), add_to_front(add_to_front) {}

RemoteAppsModel::FolderInfo::FolderInfo(const FolderInfo& other) = default;

RemoteAppsModel::FolderInfo::~FolderInfo() = default;

RemoteAppsModel::RemoteAppsModel()
    : id_generator_(std::make_unique<GuidIdGenerator>()) {}

RemoteAppsModel::~RemoteAppsModel() = default;

RemoteAppsModel::AppInfo& RemoteAppsModel::AddApp(const std::string& name,
                                                  const GURL& icon_url,
                                                  const std::string& folder_id,
                                                  bool add_to_front) {
  std::string id = id_generator_->GenerateId();
  app_map_.insert({id, AppInfo(id, name, icon_url, folder_id, add_to_front)});

  if (!folder_id.empty()) {
    DCHECK(folder_map_.find(folder_id) != folder_map_.end());
    FolderInfo& folder_info = folder_map_.at(folder_id);
    folder_info.items.insert(id);
  }

  return app_map_.at(id);
}

bool RemoteAppsModel::HasApp(const std::string& id) const {
  return app_map_.find(id) != app_map_.end();
}

RemoteAppsModel::AppInfo& RemoteAppsModel::GetAppInfo(const std::string& id) {
  DCHECK(app_map_.find(id) != app_map_.end());
  return app_map_.at(id);
}

const std::map<std::string, RemoteAppsModel::AppInfo>&
RemoteAppsModel::GetAllAppInfo() const {
  return app_map_;
}

RemoteAppsModel::FolderInfo& RemoteAppsModel::AddFolder(
    const std::string& folder_name,
    bool add_to_front) {
  std::string folder_id = id_generator_->GenerateId();
  auto it = folder_map_.insert(
      folder_map_.begin(),
      {folder_id, FolderInfo(folder_id, folder_name, add_to_front)});
  return it->second;
}

bool RemoteAppsModel::HasFolder(const std::string& folder_id) const {
  return folder_map_.find(folder_id) != folder_map_.end();
}

RemoteAppsModel::FolderInfo& RemoteAppsModel::GetFolderInfo(
    const std::string& folder_id) {
  DCHECK(folder_map_.find(folder_id) != folder_map_.end());
  return folder_map_.at(folder_id);
}

void RemoteAppsModel::DeleteApp(const std::string& id) {
  DCHECK(HasApp(id));
  auto it = app_map_.find(id);
  const std::string& folder_id = it->second.folder_id;

  if (!folder_id.empty()) {
    auto folder_it = folder_map_.find(folder_id);
    folder_it->second.items.erase(id);
  }

  app_map_.erase(it);
}

void RemoteAppsModel::DeleteFolder(const std::string& folder_id) {
  DCHECK(HasFolder(folder_id));
  auto it = folder_map_.find(folder_id);
  const std::set<std::string>& app_set = it->second.items;

  for (const auto& id : app_set) {
    DCHECK(app_map_.find(id) != app_map_.end());
    AppInfo& info = app_map_.at(id);
    info.folder_id.clear();
  }

  folder_map_.erase(it);
}

}  // namespace ash
