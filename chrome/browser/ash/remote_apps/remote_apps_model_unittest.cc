// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_model.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kId3[] = "id3";

std::unique_ptr<RemoteAppsModel> SetUpModel() {
  std::unique_ptr<RemoteAppsModel> model = std::make_unique<RemoteAppsModel>();
  std::unique_ptr<FakeIdGenerator> id_generator =
      std::make_unique<FakeIdGenerator>(
          std::vector<std::string>{kId1, kId2, kId3});
  model->SetIdGeneratorForTesting(std::move(id_generator));
  return model;
}

}  // namespace

using RemoteAppsModelUnittest = testing::Test;

TEST_F(RemoteAppsModelUnittest, AddApp) {
  const std::string name = "name";
  const GURL icon_url = GURL("icon_url");

  std::unique_ptr<RemoteAppsModel> model = SetUpModel();
  const RemoteAppsModel::AppInfo& info =
      model->AddApp(name, icon_url, std::string(), /*add_to_front=*/true);
  EXPECT_EQ(kId1, info.id);
  EXPECT_EQ(name, info.name);
  EXPECT_EQ(icon_url, info.icon_url);
  EXPECT_EQ(std::string(), info.folder_id);
  EXPECT_TRUE(info.add_to_front);
  EXPECT_TRUE(model->HasApp(info.id));

  // Check |GetAppInfo()|.
  const RemoteAppsModel::AppInfo& info2 = model->GetAppInfo(info.id);
  EXPECT_EQ(kId1, info2.id);
  EXPECT_EQ(name, info2.name);
  EXPECT_EQ(icon_url, info2.icon_url);
  EXPECT_EQ(std::string(), info2.folder_id);
  EXPECT_TRUE(info.add_to_front);

  model->DeleteApp(info.id);
  EXPECT_FALSE(model->HasApp(info.id));
}

TEST_F(RemoteAppsModelUnittest, GetAllAppInfo) {
  const std::string name = "name";
  const GURL icon_url = GURL("icon_url");
  const std::string name2 = "name2";
  const GURL icon_url2 = GURL("icon_url2");

  std::unique_ptr<RemoteAppsModel> model = SetUpModel();
  model->AddApp(name, icon_url, std::string(), /*add_to_front=*/false);
  model->AddApp(name2, icon_url2, std::string(), /*add_to_front=*/true);

  const std::map<std::string, RemoteAppsModel::AppInfo>& infos =
      model->GetAllAppInfo();
  EXPECT_EQ(2u, infos.size());

  const RemoteAppsModel::AppInfo& info = infos.at(kId1);
  EXPECT_EQ(kId1, info.id);
  EXPECT_EQ(name, info.name);
  EXPECT_EQ(icon_url, info.icon_url);
  EXPECT_FALSE(info.add_to_front);

  const RemoteAppsModel::AppInfo& info2 = infos.at(kId2);
  EXPECT_EQ(kId2, info2.id);
  EXPECT_EQ(name2, info2.name);
  EXPECT_EQ(icon_url2, info2.icon_url);
  EXPECT_TRUE(info2.add_to_front);
}

TEST_F(RemoteAppsModelUnittest, AddFolder) {
  const std::string folder_name = "folder_name";
  const std::string name = "name";
  const GURL icon_url = GURL("icon_url");

  std::unique_ptr<RemoteAppsModel> model = SetUpModel();
  const RemoteAppsModel::FolderInfo& folder_info =
      model->AddFolder(folder_name, /*add_to_front=*/true);
  const std::string folder_id = folder_info.id;
  EXPECT_EQ(kId1, folder_id);
  EXPECT_EQ(folder_name, folder_info.name);
  EXPECT_EQ(0u, folder_info.items.size());
  EXPECT_TRUE(folder_info.add_to_front);

  // Check |GetFolderInfo()|.
  const RemoteAppsModel::FolderInfo& folder_info2 =
      model->GetFolderInfo(folder_info.id);
  EXPECT_EQ(kId1, folder_id);
  EXPECT_EQ(folder_name, folder_info2.name);
  EXPECT_EQ(0u, folder_info2.items.size());
  EXPECT_TRUE(folder_info2.add_to_front);

  model->DeleteFolder(folder_id);
  EXPECT_FALSE(model->HasFolder(folder_id));
}

TEST_F(RemoteAppsModelUnittest, FolderWithMultipleApps) {
  const std::string folder_name = "folder_name";
  const std::string name = "name";
  const GURL icon_url = GURL("icon_url");

  std::unique_ptr<RemoteAppsModel> model = SetUpModel();
  const RemoteAppsModel::FolderInfo& folder_info =
      model->AddFolder(folder_name, /*add_to_front=*/false);
  std::string folder_id = folder_info.id;
  EXPECT_EQ(kId1, folder_info.id);
  EXPECT_EQ(folder_name, folder_info.name);
  EXPECT_EQ(0u, folder_info.items.size());
  EXPECT_TRUE(model->HasFolder(folder_id));

  const RemoteAppsModel::AppInfo& info =
      model->AddApp(name, icon_url, folder_id, /*add_to_front=*/false);
  EXPECT_EQ(kId2, info.id);
  EXPECT_EQ(folder_id, info.folder_id);
  EXPECT_EQ(1u, folder_info.items.size());
  EXPECT_EQ(1u, folder_info.items.count(info.id));

  // Add second app.
  const RemoteAppsModel::AppInfo& info2 =
      model->AddApp(name, icon_url, folder_id, /*add_to_front=*/false);
  EXPECT_EQ(kId3, info2.id);
  EXPECT_EQ(2u, folder_info.items.size());
  EXPECT_EQ(1u, folder_info.items.count(info2.id));

  // Check that app is removed from folder when app deleted.
  model->DeleteApp(info.id);
  EXPECT_EQ(1u, folder_info.items.size());

  // Check that app is removed from folder when folder is deleted.
  model->DeleteFolder(folder_id);
  EXPECT_EQ(std::string(), info2.folder_id);
}

}  // namespace ash
