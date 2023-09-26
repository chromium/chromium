// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_database.h"

#include <memory>
#include <string>

#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "chrome/browser/android/webapk/fake_webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registrar.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {

// Note: this only compares basic sync attributes, it doesn't do a full deep
// comparison.
bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size()) {
    return false;
  }

  for (auto& kv : registry) {
    const WebApkProto* web_app = kv.second.get();
    const WebApkProto* web_app2 = registry2.at(kv.first).get();

    const sync_pb::WebApkSpecifics& specifics = web_app->sync_data();
    const sync_pb::WebApkSpecifics& specifics2 = web_app2->sync_data();

    if (web_app->is_locally_installed() != web_app2->is_locally_installed() ||
        specifics.manifest_id() != specifics2.manifest_id() ||
        specifics.start_url() != specifics2.start_url() ||
        specifics.name() != specifics2.name()) {
      return false;
    }
  }

  return true;
}

class WebApkDatabaseTest : public ::testing::Test {
 public:
  WebApkDatabaseTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, true);

    database_factory_ = std::make_unique<FakeWebApkDatabaseFactory>();

    web_contents_ = web_contents_factory_.CreateWebContents(profile());
  };

  // Creates a random WebApkProto based off of a suffix.
  static std::unique_ptr<WebApkProto> CreateWebApkProto(
      int suffix,
      bool is_locally_installed) {
    const std::string start_url =
        "https://example.com/" + base::ToString(suffix);
    const std::string manifest_id =
        "https://example.com/id/" + base::ToString(suffix);
    const std::string name = "App Name " + base::ToString(suffix);

    std::unique_ptr<WebApkProto> proto = std::make_unique<WebApkProto>();
    sync_pb::WebApkSpecifics* sync_proto = proto->mutable_sync_data();

    sync_proto->set_manifest_id(manifest_id);
    sync_proto->set_start_url(GURL(start_url).spec());
    sync_proto->set_name(name);
    proto->set_is_locally_installed(is_locally_installed);

    return proto;
  }

  void WriteBatch(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory().GetStore()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const absl::optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(uint32_t num_apps) {
    Registry registry;

    auto write_batch = database_factory().GetStore()->CreateWriteBatch();

    for (uint32_t i = 0; i < num_apps; ++i) {
      std::unique_ptr<WebApkProto> proto = CreateWebApkProto(i, false);
      const webapps::AppId app_id =
          GenerateAppIdFromManifestId(GURL(proto->sync_data().manifest_id()));

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(proto));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  FakeWebApkDatabaseFactory& database_factory() { return *database_factory_; }
  FakeWebApkDatabaseFactory* database_factory_ptr() {
    return database_factory_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeWebApkDatabaseFactory> database_factory_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  raw_ptr<TestingProfile> profile_;

  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

TEST_F(WebApkDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps(100);

  std::unique_ptr<WebApkDatabase> web_apk_database;

  web_apk_database = std::make_unique<WebApkDatabase>(
      database_factory_ptr(),
      base::BindLambdaForTesting([&](const syncer::ModelError& error) {
        ASSERT_TRUE(false);  // should not be reached
      }));

  {
    base::RunLoop run_loop;
    web_apk_database->OpenDatabase(base::BindLambdaForTesting(
        [&](Registry inner_registry,
            std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
          EXPECT_TRUE(IsRegistryEqual(inner_registry, registry));
          // TODO(hartmanng): it might be a good idea to test the metadata_batch
          // here too, although currently it's empty.
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace webapk
