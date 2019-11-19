// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_extensions_external_loader.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Information about found external extension file: {version, crx_path}.
using TestCrxInfo = std::tuple<std::string, std::string>;

constexpr char kTestExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

constexpr char kTestExtensionUpdateManifest[] =
    "extensions/good_v1_update_manifest.xml";

constexpr char kTestExtensionCRXVersion[] = "1.0.0.0";

class TestExternalProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  TestExternalProviderVisitor() = default;
  ~TestExternalProviderVisitor() override = default;

  const std::map<std::string, TestCrxInfo>& loaded_crx_files() const {
    return loaded_crx_files_;
  }

  void WaitForReady() {
    if (ready_)
      return;
    ready_waiter_ = std::make_unique<base::RunLoop>();
    ready_waiter_->Run();
    ready_waiter_.reset();
  }

  void WaitForFileFound() {
    if (!loaded_crx_files_.empty())
      return;
    file_waiter_ = std::make_unique<base::RunLoop>();
    file_waiter_->Run();
    file_waiter_.reset();
  }

  void ClearLoadedFiles() { loaded_crx_files_.clear(); }

  // extensions::ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const extensions::ExternalInstallInfoFile& info) override {
    EXPECT_EQ(0u, loaded_crx_files_.count(info.extension_id));
    EXPECT_EQ(extensions::Manifest::INTERNAL, info.crx_location)
        << info.extension_id;

    loaded_crx_files_.emplace(
        info.extension_id,
        TestCrxInfo(info.version.GetString(), info.path.value()));
    if (file_waiter_)
      file_waiter_->Quit();
    return true;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const extensions::ExternalInstallInfoUpdateUrl& info,
      bool is_initial_load) override {
    return true;
  }

  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {
    ready_ = true;
    if (ready_waiter_)
      ready_waiter_->Quit();
  }

  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<extensions::ExternalInstallInfoUpdateUrl>&
          update_url_extensions,
      const std::vector<extensions::ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "Found updated extensions.";
  }

 private:
  bool ready_ = false;

  std::map<std::string, TestCrxInfo> loaded_crx_files_;

  std::unique_ptr<base::RunLoop> ready_waiter_;

  std::unique_ptr<base::RunLoop> file_waiter_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalProviderVisitor);
};

}  // namespace

class DemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  DemoExtensionsExternalLoaderTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        scoped_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  ~DemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override {
    demo_mode_test_helper_ = std::make_unique<DemoModeTestHelper>();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_loader_factory_);
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    profile_.reset();
    demo_mode_test_helper_.reset();
  }

 protected:
  std::string GetTestResourcePath(const std::string& rel_path) {
    return demo_mode_test_helper_->GetDemoResourcesPath()
        .Append(rel_path)
        .value();
  }

  bool SetExtensionsConfig(const base::Value& config) {
    std::string config_str;
    if (!base::JSONWriter::Write(config, &config_str))
      return false;

    base::FilePath config_path =
        demo_mode_test_helper_->GetDemoResourcesPath().Append(
            "demo_extensions.json");
    int written =
        base::WriteFile(config_path, config_str.data(), config_str.size());
    return written == static_cast<int>(config_str.size());
  }

  void AddExtensionToConfig(const std::string& id,
                            const base::Optional<std::string>& version,
                            const base::Optional<std::string>& path,
                            base::Value* config) {
    ASSERT_TRUE(config->is_dict());

    base::Value extension(base::Value::Type::DICTIONARY);
    if (version.has_value()) {
      extension.SetKey(extensions::ExternalProviderImpl::kExternalVersion,
                       base::Value(version.value()));
    }
    if (path.has_value()) {
      extension.SetKey(extensions::ExternalProviderImpl::kExternalCrx,
                       base::Value(path.value()));
    }
    config->SetKey(id, std::move(extension));
  }

  std::unique_ptr<extensions::ExternalProviderImpl> CreateExternalProvider(
      extensions::ExternalProviderInterface::VisitorInterface* visitor) {
    return std::make_unique<extensions::ExternalProviderImpl>(
        visitor,
        base::MakeRefCounted<DemoExtensionsExternalLoader>(
            base::FilePath() /*cache_dir*/),
        profile_.get(), extensions::Manifest::INTERNAL,
        extensions::Manifest::INTERNAL,
        extensions::Extension::FROM_WEBSTORE |
            extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  }

  TestExternalProviderVisitor external_provider_visitor_;

  std::unique_ptr<TestingProfile> profile_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<DemoModeTestHelper> demo_mode_test_helper_;

  content::BrowserTaskEnvironment task_environment_;

 private:
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  user_manager::ScopedUserManager scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(DemoExtensionsExternalLoaderTest);
};

TEST_F(DemoExtensionsExternalLoaderTest, NoDemoExtensionsConfig) {
  demo_mode_test_helper_->InitializeSession();

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, InvalidDemoExtensionsConfig) {
  demo_mode_test_helper_->InitializeSession();

  ASSERT_TRUE(SetExtensionsConfig(base::Value("invalid_config")));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, SingleDemoExtension) {
  demo_mode_test_helper_->InitializeSession();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("extensions/a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))}};
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, MultipleDemoExtension) {
  demo_mode_test_helper_->InitializeSession();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("extensions/a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::make_optional("b.crx"), &config);
  AddExtensionToConfig(std::string(32, 'c'), base::make_optional("2.0.0"),
                       base::make_optional("c.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))},
      {std::string(32, 'b'),
       TestCrxInfo("1.1.0", GetTestResourcePath("b.crx"))},
      {std::string(32, 'c'),
       TestCrxInfo("2.0.0", GetTestResourcePath("c.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, CrxPathWithAbsolutePath) {
  demo_mode_test_helper_->InitializeSession();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::make_optional(GetTestResourcePath("b.crx")),
                       &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithPathMissing) {
  demo_mode_test_helper_->InitializeSession();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::nullopt, &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithVersionMissing) {
  demo_mode_test_helper_->InitializeSession();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::nullopt,
                       base::make_optional("b.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, DemoResourcesNotLoaded) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();
  demo_mode_test_helper_->FailLoadingComponent();

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoaded) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  demo_mode_test_helper_->FinishLoadingComponent();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoadFails) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  demo_mode_test_helper_->FailLoadingComponent();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, LoadApp) {
  demo_mode_test_helper_->InitializeSession();

  // Create a temporary cache directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath cache_dir = temp_dir.GetPath().Append("cache");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(cache_dir, nullptr /*error*/));

  scoped_refptr<chromeos::DemoExtensionsExternalLoader> loader =
      base::MakeRefCounted<chromeos::DemoExtensionsExternalLoader>(cache_dir);
  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      std::make_unique<extensions::ExternalProviderImpl>(
          &external_provider_visitor_, loader, profile_.get(),
          extensions::Manifest::INTERNAL,
          extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
          extensions::Extension::FROM_WEBSTORE |
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());

  loader->LoadApp(kTestExtensionId);
  // Verify that a downloader has started and is attempting to download an
  // update manifest.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Return a manifest to the downloader.
  std::string manifest;
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  EXPECT_TRUE(base::ReadFileToString(
      test_dir.Append(kTestExtensionUpdateManifest), &manifest));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->at(0).request.url.spec(),
      manifest);

  // Wait for the manifest to be parsed.
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllSources())
      .Wait();

  // Verify that the downloader is attempting to download a CRX file.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Trigger downloading of the CRX file.
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->at(0).request.url.spec(),
      "Dummy content.");

  // Verify that the CRX file exists in the cache directory.
  external_provider_visitor_.WaitForFileFound();
  const base::FilePath cached_crx_path = cache_dir.Append(base::StringPrintf(
      "%s-%s.crx", kTestExtensionId, kTestExtensionCRXVersion));
  const std::map<std::string, TestCrxInfo> expected_info = {
      {kTestExtensionId,
       TestCrxInfo(kTestExtensionCRXVersion, cached_crx_path.value())}};
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());

  // Verify that loading the app again succeeds without downloading.
  test_url_loader_factory_.ClearResponses();
  external_provider_visitor_.ClearLoadedFiles();
  loader->LoadApp(kTestExtensionId);
  external_provider_visitor_.WaitForFileFound();
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

class ShouldCreateDemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  ShouldCreateDemoExtensionsExternalLoaderTest() {
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  ~ShouldCreateDemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override {
    demo_mode_test_helper_ = std::make_unique<DemoModeTestHelper>();
  }

  void TearDown() override { demo_mode_test_helper_.reset(); }

 protected:
  std::unique_ptr<TestingProfile> AddTestUser(const AccountId& account_id) {
    auto profile = std::make_unique<TestingProfile>();
    profile->set_profile_name(account_id.GetUserEmail());
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    return profile;
  }

  void StartDemoSession(DemoSession::DemoModeConfig demo_config) {
    ASSERT_NE(DemoSession::DemoModeConfig::kNone, demo_config);
    demo_mode_test_helper_->InitializeSession();
  }

  // Owned by scoped_user_manager_.
  FakeChromeUserManager* user_manager_ = nullptr;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<DemoModeTestHelper> demo_mode_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ShouldCreateDemoExtensionsExternalLoaderTest);
};

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, PrimaryDemoProfile) {
  StartDemoSession(DemoSession::DemoModeConfig::kOnline);

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_TRUE(DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest,
       PrimaryOfflineEnrolledDemoProfile) {
  StartDemoSession(DemoSession::DemoModeConfig::kOffline);

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_TRUE(DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, ProfileWithNoUser) {
  StartDemoSession(DemoSession::DemoModeConfig::kOnline);
  TestingProfile profile;

  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(&profile));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, MultiProfile) {
  StartDemoSession(DemoSession::DemoModeConfig::kOnline);

  std::unique_ptr<TestingProfile> primary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  std::unique_ptr<TestingProfile> secondary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("secondary@test.com", "secondary_user"));

  EXPECT_TRUE(
      DemoExtensionsExternalLoader::SupportedForProfile(primary_profile.get()));
  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(
      secondary_profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, NotDemoMode) {
  // This should be no-op, given that the default demo session enrollment state
  // is not-enrolled.
  DemoSession::StartIfInDemoMode();
  ASSERT_FALSE(DemoSession::Get());

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, DemoSessionNotStarted) {
  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

}  // namespace chromeos
