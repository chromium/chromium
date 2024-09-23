// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_test_util.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

namespace app_list {

const char AppListTestBase::kHostedAppId[] = "dceacbkfkmllgmjmbhgkpjegnodmildf";
const char AppListTestBase::kPackagedApp1Id[] =
    "emfkafnhnpcmabnnkckkchdilgeoekbo";
const char AppListTestBase::kPackagedApp2Id[] =
    "jlklkagmeajbjiobondfhiekepofmljl";

AppListTestBase::AppListTestBase() {}

AppListTestBase::~AppListTestBase() {}

void AppListTestBase::SetUp() {
  SetUp(/*guest_mode=*/false);
}

void AppListTestBase::SetUp(bool guest_mode) {
  extensions::ExtensionServiceTestBase::SetUp();

  // Load "app_list" extensions test profile.
  // The test profile has 4 extensions:
  // - 1 dummy extension (which should not be visible in the launcher)
  // - 2 packaged extension apps
  // - 1 hosted extension app
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("app_list")));
  params.profile_is_guest = guest_mode;
  InitializeExtensionService(std::move(params));
  service_->Init();

  ConfigureWebAppProvider();

  // Let any async services complete their set-up.
  base::RunLoop().RunUntilIdle();

  // There should be 4 extensions in the test profile.
  ASSERT_EQ(4U, registry()->enabled_extensions().size());
}

void AppListTestBase::ConfigureWebAppProvider() {
  Profile* testing_profile = profile();

  auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
  url_loader_ = url_loader.get();

  auto externally_managed_app_manager =
      std::make_unique<web_app::ExternallyManagedAppManager>(testing_profile);
  externally_managed_app_manager->SetUrlLoaderForTesting(std::move(url_loader));

  auto* const provider = web_app::FakeWebAppProvider::Get(testing_profile);
  provider->SetExternallyManagedAppManager(
      std::move(externally_managed_app_manager));
  web_app::test::AwaitStartWebAppProviderAndSubsystems(testing_profile);
}

// Test util constants ---------------------------------------------------------

const char kUnset[] = "__unset__";
const char kDefault[] = "__default__";
const char kOemAppName[] = "oem_app";
const char kSomeAppName[] = "some_app";

// Test util functions----------------------------------------------------------

scoped_refptr<extensions::Extension> MakeApp(
    const std::string& name,
    const std::string& id,
    extensions::Extension::InitFromValueFlags flags) {
  std::string err;
  base::Value::Dict value;
  value.Set("name", name);
  value.Set("version", "0.0");
  value.SetByDottedPath("app.launch.web_url", "http://google.com");
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      flags, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

std::string CreateNextAppId(const std::string& app_id) {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
  std::string next_app_id = app_id;
  size_t index = next_app_id.length() - 1;
  while (index > 0 && next_app_id[index] == 'p')
    next_app_id[index--] = 'a';
  DCHECK_NE(next_app_id[index], 'p');
  next_app_id[index]++;
  DCHECK(crx_file::id_util::IdIsValid(next_app_id));
  return next_app_id;
}

syncer::SyncData CreateAppRemoteData(
    const std::string& id,
    const std::string& name,
    const std::string& parent_id,
    const std::string& item_ordinal,
    const std::string& item_pin_ordinal,
    sync_pb::AppListSpecifics_AppListItemType item_type,
    std::optional<bool> is_user_pinned,
    const std::string& promise_package_id) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AppListSpecifics* app_list = specifics.mutable_app_list();
  if (id != kUnset)
    app_list->set_item_id(id);
  app_list->set_item_type(item_type);
  if (name != kUnset)
    app_list->set_item_name(name);
  if (parent_id != kUnset)
    app_list->set_parent_id(parent_id);
  if (item_ordinal != kUnset)
    app_list->set_item_ordinal(item_ordinal);
  if (item_pin_ordinal != kUnset)
    app_list->set_item_pin_ordinal(item_pin_ordinal);
  if (is_user_pinned.has_value()) {
    app_list->set_is_user_pinned(*is_user_pinned);
  }
  if (promise_package_id != kUnset) {
    app_list->set_promise_package_id(promise_package_id);
  }

  return syncer::SyncData::CreateRemoteData(
      specifics, syncer::ClientTagHash::FromHashed("unused"));
}

}  // namespace app_list
