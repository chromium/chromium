// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"
#include "android_webview/nonembedded/webview_apk_process.h"
#include "base/android/path_utils.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/network.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

constexpr char kComponentId[] = "jebgalgnebhfojomionfpkfelancnnkf";
// This hash corresponds to kComponentId.
constexpr uint8_t kSha256Hash[] = {
    0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
    0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
    0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

constexpr char kManifestJson[] =
    "{"
    "\n\"manifest_version\": 2,"
    "\n\"name\": \"jebgalgnebhfojomionfpkfelancnnkf\","
    "\n\"version\": \"123.456.789\""
    "\n}";

constexpr char kTestVersion[] = "123.456.789";

void CreateTestFiles(const base::FilePath& install_dir) {
  base::CreateDirectory(install_dir);
  ASSERT_TRUE(base::WriteFile(install_dir.AppendASCII("file1.txt"), "1"));
  ASSERT_TRUE(
      base::WriteFile(install_dir.AppendASCII("manifest.json"), kManifestJson));
}

class FailingNetworkFetcher : public update_client::NetworkFetcher {
 public:
  FailingNetworkFetcher() = default;
  ~FailingNetworkFetcher() override = default;
  FailingNetworkFetcher(const FailingNetworkFetcher&) = delete;
  FailingNetworkFetcher& operator=(const FailingNetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    std::move(post_request_complete_callback)
        .Run(/* response_body= */ std::make_unique<std::string>(""),
             /* network_error= */ -2,
             /* header_etag= */ "",
             /* header_x_cup_server_proof= */ "",
             /* x_header_retry_after_sec= */ 0ll);
  }

  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      ProgressCallback progress_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override {
    std::move(download_to_file_complete_callback)
        .Run(
            /* network_error= */ -2,
            /* content_size= */ 0);
  }
};

class FailingNetworkFetcherFactory
    : public update_client::NetworkFetcherFactory {
 public:
  FailingNetworkFetcherFactory() = default;
  FailingNetworkFetcherFactory(const FailingNetworkFetcherFactory&) = delete;
  FailingNetworkFetcherFactory& operator=(const FailingNetworkFetcherFactory&) =
      delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override {
    return std::make_unique<FailingNetworkFetcher>();
  }

 protected:
  ~FailingNetworkFetcherFactory() override = default;
};

class MockConfigurator : public AwComponentUpdaterConfigurator {
 public:
  explicit MockConfigurator(const base::CommandLine* cmdline,
                            PrefService* pref_service)
      : AwComponentUpdaterConfigurator(cmdline, pref_service) {}
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override {
    return base::MakeRefCounted<FailingNetworkFetcherFactory>();
  }

 protected:
  ~MockConfigurator() override = default;
};

class MockInstallerPolicy : public component_updater::ComponentInstallerPolicy {
 public:
  MockInstallerPolicy() = default;
  ~MockInstallerPolicy() override = default;

  MockInstallerPolicy(const MockInstallerPolicy&) = delete;
  MockInstallerPolicy& operator=(const MockInstallerPolicy&) = delete;

  // Overridden ComponentInstallerPolicy methods
  bool SupportsGroupPolicyEnabledComponentUpdates() const override {
    return false;
  }
  bool RequiresNetworkEncryption() const override { return false; }

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(0);
  }

  void OnCustomUninstall() override { FAIL(); }

  void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) override {
    version_ = version;
    install_dir_ = install_dir;
    manifest_ = std::move(manifest);
  }

  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override {
    return true;
  }

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(FILE_PATH_LITERAL(kComponentId));
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
  }

  std::string GetName() const override { return kComponentId; }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    return update_client::InstallerAttributes();
  }

  base::DictionaryValue* GetManifest() { return manifest_.get(); }
  base::FilePath GetInstallDir() const { return install_dir_; }
  base::Version GetVersion() const { return version_; }

 private:
  std::unique_ptr<base::DictionaryValue> manifest_;
  base::FilePath install_dir_;
  base::Version version_;
};

}  // namespace

class TestAwComponentUpdateService : public AwComponentUpdateService {
 public:
  explicit TestAwComponentUpdateService(
      scoped_refptr<update_client::Configurator> configurator)
      : AwComponentUpdateService(configurator) {}

  ~TestAwComponentUpdateService() override = default;

  MockInstallerPolicy* GetMockPolicy() { return mock_policy_; }

 protected:
  void RegisterComponents(RegisterComponentsCallback register_callback,
                          base::OnceClosure on_finished) override {
    auto policy = std::make_unique<MockInstallerPolicy>();
    mock_policy_ = policy.get();

    base::MakeRefCounted<component_updater::ComponentInstaller>(
        std::move(policy))
        ->Register(std::move(register_callback), std::move(on_finished));
  }

 private:
  MockInstallerPolicy* mock_policy_;
};

class AwComponentUpdateServiceTest : public testing::Test {
 public:
  AwComponentUpdateServiceTest() = default;
  ~AwComponentUpdateServiceTest() override = default;

  AwComponentUpdateServiceTest(const AwComponentUpdateServiceTest&) = delete;
  AwComponentUpdateServiceTest& operator=(const AwComponentUpdateServiceTest&) =
      delete;

  static void SetUpTestSuite() {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
        "ComponentInstallerPolicyDelegateTest");
  }

  // Override from testing::Test
  void SetUp() override {
    ASSERT_TRUE(base::android::GetDataDirectory(&component_install_dir_));
    component_install_dir_ = component_install_dir_.AppendASCII("components")
                                 .AppendASCII("cus")
                                 .AppendASCII(kComponentId);
  }

  void TearDown() override {
    if (base::PathExists(component_install_dir_))
      ASSERT_TRUE(base::DeletePathRecursively(component_install_dir_));
  }

 protected:
  base::FilePath component_install_dir_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AwComponentUpdateServiceTest, TestComponentReadyWhenOffline) {
  CreateTestFiles(component_install_dir_.AppendASCII(kTestVersion));

  TestAwComponentUpdateService service(base::MakeRefCounted<MockConfigurator>(
      base::CommandLine::ForCurrentProcess(),
      WebViewApkProcess::GetInstance()->GetPrefService()));

  base::RunLoop run_loop;
  service.StartComponentUpdateService(run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(service.GetMockPolicy()->GetVersion().GetString(), kTestVersion);
  EXPECT_EQ(service.GetMockPolicy()->GetInstallDir(),
            component_install_dir_.AppendASCII(kTestVersion));
}

}  // namespace android_webview
