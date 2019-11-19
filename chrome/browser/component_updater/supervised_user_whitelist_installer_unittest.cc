// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using update_client::CrxComponent;
using update_client::CrxUpdateItem;

namespace component_updater {

namespace {

const char kClientId[] = "client-id";
const char kCrxId[] = "abcdefghijklmnopponmlkjihgfedcba";
const char kName[] = "Some Whitelist";
const char kOtherClientId[] = "other-client-id";
const char kVersion[] = "1.2.3.4";
const char kWhitelistContents[] = "{\"foo\": \"bar\"}";
const char kWhitelistFile[] = "whitelist.json";
const char kLargeIconFile[] = "icon.png";

std::string CrxIdToHashToCrxId(const std::string& kCrxId) {
  CrxComponent component;
  component.pk_hash =
      SupervisedUserWhitelistInstaller::GetHashFromCrxId(kCrxId);
  EXPECT_EQ(16u, component.pk_hash.size());
  return GetCrxComponentID(component);
}

std::string JsonToString(const base::DictionaryValue& dict) {
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

class MockComponentUpdateService : public ComponentUpdateService,
                                   public OnDemandUpdater {
 public:
  ~MockComponentUpdateService() override {}

  bool on_demand_update_called() const { return on_demand_update_called_; }

  const CrxComponent* registered_component() { return component_.get(); }

  void set_registration_callback(const base::Closure& registration_callback) {
    registration_callback_ = registration_callback;
  }

  // ComponentUpdateService implementation:
  void AddObserver(Observer* observer) override { ADD_FAILURE(); }
  void RemoveObserver(Observer* observer) override { ADD_FAILURE(); }

  std::vector<std::string> GetComponentIDs() const override {
    ADD_FAILURE();
    return std::vector<std::string>();
  }

  bool RegisterComponent(const CrxComponent& component) override {
    EXPECT_EQ(nullptr, component_.get());
    component_ = std::make_unique<CrxComponent>(component);
    if (!registration_callback_.is_null())
      registration_callback_.Run();

    return true;
  }

  bool UnregisterComponent(const std::string& crx_id) override {
    if (!component_) {
      ADD_FAILURE();
      return false;
    }

    EXPECT_EQ(GetCrxComponentID(*component_), crx_id);
    if (!component_->installer->Uninstall()) {
      ADD_FAILURE();
      return false;
    }

    component_.reset();
    return true;
  }

  OnDemandUpdater& GetOnDemandUpdater() override { return *this; }

  void MaybeThrottle(const std::string& kCrxId,
                     const base::OnceClosure callback) override {
    ADD_FAILURE();
  }

  bool GetComponentDetails(const std::string& component_id,
                           CrxUpdateItem* item) const override {
    ADD_FAILURE();
    return false;
  }

  std::unique_ptr<ComponentInfo> GetComponentForMimeType(
      const std::string& mime_type) const override {
    return nullptr;
  }

  std::vector<ComponentInfo> GetComponents() const override {
    return std::vector<ComponentInfo>();
  }

  // OnDemandUpdater implementation:
  void OnDemandUpdate(const std::string& crx_id,
                      Priority priority,
                      Callback callback) override {
    on_demand_update_called_ = true;

    if (!component_) {
      ADD_FAILURE() << "Trying to update unregistered component " << crx_id;
      return;
    }

    EXPECT_EQ(OnDemandUpdater::Priority::FOREGROUND, priority);
    EXPECT_EQ(GetCrxComponentID(*component_), crx_id);
  }

 private:
  std::unique_ptr<CrxComponent> component_;
  base::Closure registration_callback_;
  bool on_demand_update_called_ = false;
};

class WhitelistLoadObserver {
 public:
  explicit WhitelistLoadObserver(SupervisedUserWhitelistInstaller* installer) {
    installer->Subscribe(base::Bind(&WhitelistLoadObserver::OnWhitelistReady,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void Wait() { run_loop_.Run(); }
  void Quit() { run_loop_.Quit(); }

  const base::FilePath& large_icon_path() const { return large_icon_path_; }
  const base::FilePath& whitelist_path() const { return whitelist_path_; }

 private:
  void OnWhitelistReady(const std::string& crx_id,
                        const base::string16& title,
                        const base::FilePath& large_icon_path,
                        const base::FilePath& whitelist_path) {
    EXPECT_EQ(base::FilePath::StringType(), large_icon_path_.value());
    EXPECT_EQ(base::FilePath::StringType(), whitelist_path_.value());
    whitelist_path_ = whitelist_path;
    large_icon_path_ = large_icon_path;
    Quit();
  }

  base::FilePath large_icon_path_;
  base::FilePath whitelist_path_;

  base::RunLoop run_loop_;
  base::WeakPtrFactory<WhitelistLoadObserver> weak_ptr_factory_{this};
};

}  // namespace

class SupervisedUserWhitelistInstallerTest : public testing::Test {
 public:
  SupervisedUserWhitelistInstallerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~SupervisedUserWhitelistInstallerTest() override {}

  void SetUp() override {
    SupervisedUserWhitelistInstaller::RegisterPrefs(local_state_.registry());

    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_attributes_storage()->AddProfile(
        GetProfilePath(kClientId), base::ASCIIToUTF16("A Profile"),
        std::string(), base::string16(), false, 0, std::string(),
        EmptyAccountId());
    profile_attributes_storage()->AddProfile(
        GetProfilePath(kOtherClientId), base::ASCIIToUTF16("Another Profile"),
        std::string(), base::string16(), false, 0, std::string(),
        EmptyAccountId());

    installer_ = SupervisedUserWhitelistInstaller::Create(
        &component_update_service_,
        profile_attributes_storage(),
        &local_state_);

    ASSERT_TRUE(base::PathService::Get(DIR_SUPERVISED_USER_WHITELISTS,
                                       &whitelist_base_directory_));
    whitelist_directory_ = whitelist_base_directory_.AppendASCII(kCrxId);
    whitelist_version_directory_ = whitelist_directory_.AppendASCII(kVersion);

    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS,
                               &installed_whitelist_directory_));
    std::string crx_id(kCrxId);
    whitelist_path_ =
        installed_whitelist_directory_.AppendASCII(crx_id + ".json");
    large_icon_path_ = whitelist_version_directory_.AppendASCII(kLargeIconFile);

    auto crx_dict = std::make_unique<base::DictionaryValue>();
    crx_dict->SetString("name", kName);
    std::unique_ptr<base::ListValue> clients =
        std::make_unique<base::ListValue>();
    clients->AppendString(kClientId);
    clients->AppendString(kOtherClientId);
    crx_dict->Set("clients", std::move(clients));
    pref_.Set(kCrxId, std::move(crx_dict));
  }

 protected:
  ProfileAttributesStorage* profile_attributes_storage() {
    return testing_profile_manager_.profile_attributes_storage();
  }

  base::FilePath GetProfilePath(const std::string& profile_name) {
    return testing_profile_manager_.profiles_dir().AppendASCII(profile_name);
  }

  void PrepareWhitelistFile(const base::FilePath& whitelist_path) {
    size_t whitelist_contents_length = sizeof(kWhitelistContents) - 1;
    ASSERT_EQ(static_cast<int>(whitelist_contents_length),
              base::WriteFile(whitelist_path, kWhitelistContents,
                              whitelist_contents_length));
  }

  void PrepareWhitelistDirectory(const base::FilePath& whitelist_directory) {
    PrepareWhitelistFile(whitelist_directory.AppendASCII(kWhitelistFile));
    base::FilePath manifest_file =
        whitelist_directory.AppendASCII("manifest.json");

    base::DictionaryValue manifest;

    auto whitelist_dict = std::make_unique<base::DictionaryValue>();
    whitelist_dict->SetString("sites", kWhitelistFile);
    manifest.Set("whitelisted_content", std::move(whitelist_dict));

    auto icons_dict = std::make_unique<base::DictionaryValue>();
    icons_dict->SetString("128", kLargeIconFile);
    manifest.Set("icons", std::move(icons_dict));

    manifest.SetString("version", kVersion);

    ASSERT_TRUE(JSONFileValueSerializer(manifest_file).Serialize(manifest));
  }

  void RegisterExistingComponents() {
    local_state_.Set(prefs::kRegisteredSupervisedUserWhitelists, pref_);
    installer_->RegisterComponents();
    content::RunAllTasksUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void CheckRegisteredComponent(const char* version) {
    const CrxComponent* component =
        component_update_service_.registered_component();
    ASSERT_TRUE(component);
    EXPECT_EQ(kName, component->name);
    EXPECT_EQ(kCrxId, GetCrxComponentID(*component));
    EXPECT_EQ(version, component->version.GetString());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<SupervisedUserWhitelistInstaller> installer_;
  base::FilePath whitelist_base_directory_;
  base::FilePath whitelist_directory_;
  base::FilePath whitelist_version_directory_;
  base::FilePath installed_whitelist_directory_;
  base::FilePath whitelist_path_;
  base::FilePath large_icon_path_;
  base::DictionaryValue pref_;
  MockComponentUpdateService component_update_service_;
};

TEST_F(SupervisedUserWhitelistInstallerTest, GetHashFromCrxId) {
  {
    std::string extension_id = "abcdefghijklmnopponmlkjihgfedcba";
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = "aBcDeFgHiJkLmNoPpOnMlKjIhGfEdCbA";
    ASSERT_EQ(base::ToLowerASCII(extension_id),
              CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = crx_file::id_util::GenerateId("Moose");
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }
}

TEST_F(SupervisedUserWhitelistInstallerTest, InstallNewWhitelist) {
  base::RunLoop registration_run_loop;
  component_update_service_.set_registration_callback(
      registration_run_loop.QuitClosure());

  WhitelistLoadObserver observer(installer_.get());
  installer_->RegisterWhitelist(kClientId, kCrxId, kName);
  registration_run_loop.Run();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent("0.0.0.0"));
  EXPECT_TRUE(component_update_service_.on_demand_update_called());

  // Registering the same whitelist for another client should not do anything.
  installer_->RegisterWhitelist(kOtherClientId, kCrxId, kName);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unpacked_path = temp_dir.GetPath();
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(unpacked_path));

  const CrxComponent* component =
      component_update_service_.registered_component();
  ASSERT_TRUE(component);

  // The lambda function argument below is called by the ComponentInstaller
  // implementation of the SupervisedUserWhitelistInstaller. Quit the observer
  // in case of errors to allow the test to continue, since the component
  // installer only calls |ComponentReady| if the install of the component
  // has succeeded.
  component->installer->Install(
      unpacked_path, std::string(),
      base::Bind(
          [](WhitelistLoadObserver* observer,
             const update_client::CrxInstaller::Result& result) {
            EXPECT_EQ(0, result.error);
            EXPECT_EQ(0, result.extended_error);
            if (result.error)
              observer->Quit();
          },
          &observer));

  content::RunAllTasksUntilIdle();

  observer.Wait();
  EXPECT_EQ(whitelist_path_.value(), observer.whitelist_path().value());
  EXPECT_EQ(large_icon_path_.value(), observer.large_icon_path().value());

  std::string whitelist_contents;
  ASSERT_TRUE(base::ReadFileToString(whitelist_path_, &whitelist_contents));

  // The actual file contents don't have to be equal, but the parsed values
  // should be.
  EXPECT_TRUE(
      base::JSONReader::ReadDeprecated(kWhitelistContents)
          ->Equals(base::JSONReader::ReadDeprecated(whitelist_contents).get()))
      << kWhitelistContents << " vs. " << whitelist_contents;

  EXPECT_EQ(JsonToString(pref_),
            JsonToString(*local_state_.GetDictionary(
                prefs::kRegisteredSupervisedUserWhitelists)));
}

TEST_F(SupervisedUserWhitelistInstallerTest,
       RegisterAndUninstallExistingWhitelist) {
  ASSERT_TRUE(base::CreateDirectory(whitelist_version_directory_));
  ASSERT_NO_FATAL_FAILURE(
      PrepareWhitelistDirectory(whitelist_version_directory_));
  ASSERT_TRUE(base::CreateDirectory(installed_whitelist_directory_));
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistFile(whitelist_path_));

  // Create another whitelist directory, with an ID that is not registered.
  base::FilePath other_directory =
      whitelist_base_directory_.AppendASCII("paobncmdlekfjgihhigjfkeldmcnboap");
  ASSERT_TRUE(base::CreateDirectory(other_directory));
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(other_directory));

  // Create a directory that is not a valid whitelist directory.
  base::FilePath non_whitelist_directory =
      whitelist_base_directory_.AppendASCII("Not a whitelist");
  ASSERT_TRUE(base::CreateDirectory(non_whitelist_directory));

  RegisterExistingComponents();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent(kVersion));
  EXPECT_FALSE(component_update_service_.on_demand_update_called());

  // Check that unregistered whitelists have been removed:
  // The registered whitelist directory should still exist.
  EXPECT_TRUE(base::DirectoryExists(whitelist_directory_));

  // The other directory should be gone.
  EXPECT_FALSE(base::DirectoryExists(other_directory));

  // The non-whitelist directory should still exist as well.
  EXPECT_TRUE(base::DirectoryExists(non_whitelist_directory));

  // Unregistering for the first client should do nothing.
  {
    base::RunLoop run_loop;
    installer_->UnregisterWhitelist(kClientId, kCrxId);
    content::RunAllTasksUntilIdle();
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(component_update_service_.registered_component());
  EXPECT_TRUE(base::DirectoryExists(whitelist_version_directory_));
  EXPECT_TRUE(base::PathExists(whitelist_path_));

  // Unregistering for the second client should uninstall the whitelist.
  {
    base::RunLoop run_loop;

    // This does the same thing in our case as calling UnregisterWhitelist(),
    // but it exercises a different code path.
    profile_attributes_storage()->RemoveProfile(GetProfilePath(kOtherClientId));
    content::RunAllTasksUntilIdle();
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(component_update_service_.registered_component());
  EXPECT_FALSE(base::DirectoryExists(whitelist_directory_));
  EXPECT_FALSE(base::PathExists(whitelist_path_));
}

}  // namespace component_updater
