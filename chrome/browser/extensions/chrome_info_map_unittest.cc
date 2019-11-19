// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions").AppendASCII(dir).AppendASCII(test_file);

  JSONFileValueDeserializer deserializer(path);
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, nullptr);
  if (!result)
    return nullptr;

  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(path,
                        Manifest::INVALID_LOCATION,
                        *static_cast<base::DictionaryValue*>(result.get()),
                        Extension::NO_FLAGS,
                        &error);
  EXPECT_TRUE(extension.get()) << error;

  return extension;
}

}  // namespace

// This test lives in Chrome because it depends on hosted app permissions
// (specifically, notifications) that do not exist in src/extensions.
class ChromeInfoMapTest : public testing::Test {
 public:
  ChromeInfoMapTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests API access permissions given both extension and app URLs.
TEST_F(ChromeInfoMapTest, CheckPermissions) {
  scoped_refptr<InfoMap> info_map(new InfoMap());

  scoped_refptr<Extension> app(
      LoadManifest("manifest_tests", "valid_app.json"));
  scoped_refptr<Extension> extension(
      LoadManifest("manifest_tests", "tabs_extension.json"));

  GURL app_url("http://www.google.com/mail/foo.html");
  ASSERT_TRUE(app->is_app());
  ASSERT_TRUE(app->web_extent().MatchesURL(app_url));

  info_map->AddExtension(app.get(), base::Time(), false, false);
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  // The app should have the notifications permission, either from a
  // chrome-extension URL or from its web extent.
  const Extension* match = info_map->extensions().GetExtensionOrAppByURL(
      app->GetResourceURL("a.html"));
  EXPECT_TRUE(match &&
              match->permissions_data()->HasAPIPermission(
                  APIPermission::kNotifications));
  match = info_map->extensions().GetExtensionOrAppByURL(app_url);
  EXPECT_TRUE(match &&
              match->permissions_data()->HasAPIPermission(
                  APIPermission::kNotifications));
  EXPECT_FALSE(
      match &&
      match->permissions_data()->HasAPIPermission(APIPermission::kTab));

  // The extension should have the tabs permission.
  match = info_map->extensions().GetExtensionOrAppByURL(
      extension->GetResourceURL("a.html"));
  EXPECT_TRUE(match &&
              match->permissions_data()->HasAPIPermission(APIPermission::kTab));
  EXPECT_FALSE(match &&
               match->permissions_data()->HasAPIPermission(
                   APIPermission::kNotifications));

  // Random URL should not have any permissions.
  GURL evil_url("http://evil.com/a.html");
  match = info_map->extensions().GetExtensionOrAppByURL(evil_url);
  EXPECT_FALSE(match);
}

TEST_F(ChromeInfoMapTest, TestNotificationsDisabled) {
  scoped_refptr<InfoMap> info_map(new InfoMap());
  scoped_refptr<Extension> app(
      LoadManifest("manifest_tests", "valid_app.json"));
  info_map->AddExtension(app.get(), base::Time(), false, false);

  EXPECT_FALSE(info_map->AreNotificationsDisabled(app->id()));
  info_map->SetNotificationsDisabled(app->id(), true);
  EXPECT_TRUE(info_map->AreNotificationsDisabled(app->id()));
  info_map->SetNotificationsDisabled(app->id(), false);
}

}  // namespace extensions
