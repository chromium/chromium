// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MODEL_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MODEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/remote_apps/id_generator.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

// Class which stores the state of Remote Apps added by |RemoteAppsManager| and
// maintains the association between the apps and folders.
class RemoteAppsModel {
 public:
  struct AppInfo {
    AppInfo(const std::string& id,
            const std::string& name,
            const GURL& icon_url,
            std::string folder_id,
            bool add_to_front);
    AppInfo(const AppInfo& other);
    ~AppInfo();

    const std::string id;
    const std::string name;
    const GURL icon_url;
    std::string folder_id;
    gfx::ImageSkia icon;
    const bool add_to_front;
  };

  struct FolderInfo {
    FolderInfo(const std::string& id,
               const std::string& name,
               bool add_to_front);
    FolderInfo(const FolderInfo& other);
    ~FolderInfo();

    const std::string id;
    const std::string name;
    const bool add_to_front;
    std::set<std::string> items;
  };

  RemoteAppsModel();
  RemoteAppsModel(const RemoteAppsModel&) = delete;
  RemoteAppsModel& operator=(const RemoteAppsModel&) = delete;
  ~RemoteAppsModel();

  // Adds an app. If |folder_id| is non-empty, the caller should ensure that
  // the folder exists by calling |HasFolder()| first. The app is added to the
  // corresponding folder.
  RemoteAppsModel::AppInfo& AddApp(const std::string& name,
                                   const GURL& icon_url,
                                   const std::string& folder_id,
                                   bool add_to_front);

  // Returns true if an app with ID |id| exists in the model.
  bool HasApp(const std::string& id) const;

  // Returns the |AppInfo| of the app with ID |id|. The caller should ensure
  // that the app exists by calling |HasApp()| first.
  RemoteAppsModel::AppInfo& GetAppInfo(const std::string& id);

  // Returns a map from string |id| to |AppInfo| for all apps in the model.
  const std::map<std::string, AppInfo>& GetAllAppInfo() const;

  // Adds a folder.
  RemoteAppsModel::FolderInfo& AddFolder(const std::string& folder_name,
                                         bool add_to_front);

  // Returns true if a folder with ID |folder_id| exists.
  bool HasFolder(const std::string& folder_id) const;

  // Returns the |FolderInfo| of the folder with ID |folder_id|. The caller
  // should ensure that the folder exists by calling |HasFolder()| first.
  RemoteAppsModel::FolderInfo& GetFolderInfo(const std::string& folder_id);

  // Deletes the app with ID |id|. The caller should ensure that the app exists
  // by calling |HasApp()| first.
  void DeleteApp(const std::string& id);

  // Deletes the folder with ID |folder_id|. The caller should ensure that the
  // folder exists by calling |HasFolder()| first. All items in the folder are
  // moved out of the folder.
  void DeleteFolder(const std::string& folder_id);

  void SetIdGeneratorForTesting(std::unique_ptr<IdGenerator> id_generator) {
    id_generator_ = std::move(id_generator);
  }

 private:
  std::unique_ptr<IdGenerator> id_generator_;
  std::map<std::string, AppInfo> app_map_;
  std::map<std::string, FolderInfo> folder_map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MODEL_H_
