// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <optional>
#include <utility>

#include "android_webview/common/aw_paths.h"
#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"
#include "base/android/path_utils.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/android/url_utils.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

// Size of android_webview/test/data/components/fake_component.crx.
constexpr size_t kCrxContentLength = 3902;

constexpr char kComponentId[] = "llkgjffcdpffmhiakmfcdcblohccpfmo";

// This hash corresponds to kComponentId.
constexpr uint8_t kSha256Hash[] = {
    0xbb, 0xa6, 0x95, 0x52, 0x3f, 0x55, 0xc7, 0x80, 0xac, 0x52, 0x32,
    0x1b, 0xe7, 0x22, 0xf5, 0xce, 0x6a, 0xfd, 0x9c, 0x9e, 0xa9, 0x2a,
    0x0b, 0x50, 0x60, 0x2b, 0x7f, 0x6c, 0x64, 0x80, 0x09, 0x04};

constexpr char kTestVersion[] = "1.0.0.6";

base::FilePath GetTestFile(const std::string& file_name) {
  return base::android::GetIsolatedTestRoot()
      .AppendASCII("android_webview/test/data/components")
      .AppendASCII(file_name);
}

void CreateTestFiles(const base::FilePath& install_dir) {
  base::CreateDirectory(install_dir);
  ASSERT_TRUE(base::WriteFile(install_dir.AppendASCII("file1.txt"), "1"));
  ASSERT_TRUE(base::CopyFile(GetTestFile("fake_component_manifest.json"),
                             install_dir.AppendASCII("manifest.json")));
}

void AssertOnDemandRequest(bool on_demand, std::string post_data) {
  const auto root = base::JSONReader::Read(post_data);
  ASSERT_TRUE(root);
  const auto* request = root->GetDict().FindDict("request");
  ASSERT_TRUE(request);
  const auto& app = (*request->FindList("app"))[0].GetDict();
  if (on_demand) {
    EXPECT_EQ("ondemand", *app.FindString("installsource"));
  } else {
    EXPECT_EQ(nullptr, app.FindString("installsource"));
  }
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
    AssertOnDemandRequest(false, post_data);
    std::move(post_request_complete_callback)
        .Run(/* response_body= */ std::make_unique<std::string>(""),
             /* network_error= */ -2,
             /* header_etag= */ "",
             /* header_x_cup_server_proof= */ "",
             /* x_header_retry_after_sec= */ 0ll);
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    std::move(download_to_file_complete_callback)
        .Run(
            /* network_error= */ -2,
            /* content_size= */ 0);
    return base::DoNothing();
  }
};

// This inspects that param onDemandUpdate gets passed down the call stack.
class OnDemandNetworkFetcher : public update_client::NetworkFetcher {
 public:
  OnDemandNetworkFetcher() = default;
  ~OnDemandNetworkFetcher() override = default;
  OnDemandNetworkFetcher(const OnDemandNetworkFetcher&) = delete;
  OnDemandNetworkFetcher& operator=(const OnDemandNetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    AssertOnDemandRequest(true, post_data);
    std::move(post_request_complete_callback)
        .Run(/* response_body= */ std::make_unique<std::string>(""),
             /* network_error= */ -2,
             /* header_etag= */ "",
             /* header_x_cup_server_proof= */ "",
             /* x_header_retry_after_sec= */ 0ll);
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    std::move(download_to_file_complete_callback)
        .Run(
            /* network_error= */ -2,
            /* content_size= */ 0);
    return base::DoNothing();
  }
};

// A NetworkFetcher that fakes downloading a CRX file.
// TODO(crbug.com/40755924) use EmbeddedTestServer instead of Mocking the
// NetworkFetcher.
class FakeCrxNetworkFetcher : public update_client::NetworkFetcher {
 public:
  FakeCrxNetworkFetcher() = default;
  ~FakeCrxNetworkFetcher() override = default;
  FakeCrxNetworkFetcher(const FakeCrxNetworkFetcher&) = delete;
  FakeCrxNetworkFetcher& operator=(const FakeCrxNetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    AssertOnDemandRequest(false, post_data);
    std::move(response_started_callback)
        .Run(/* responseCode= */ 200, /* content_size= */ 0);
    std::string response_body;
    int network_error = 0;
    if (base::Contains(post_data, "updatecheck")) {
      ASSERT_TRUE(base::ReadFileToString(
          GetTestFile("fake_component_update_response.json"), &response_body));
    } else if (base::Contains(post_data, "eventtype")) {
      ASSERT_TRUE(base::ReadFileToString(
          GetTestFile("fake_component_ping_response.json"), &response_body));
    } else {  // error post request not a ping nor update.
      network_error = -2;
    }
    std::move(post_request_complete_callback)
        .Run(/* response_body= */ std::make_unique<std::string>(response_body),
             /* network_error= */ network_error,
             /* header_etag= */ "",
             /* header_x_cup_server_proof= */ "",
             /* x_header_retry_after_sec= */ 0ll);
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    EXPECT_TRUE(base::CopyFile(GetTestFile("fake_component.crx"), file_path));
    std::move(response_started_callback)
        .Run(/* responseCode= */ 200, /* content_size= */ kCrxContentLength);
    std::move(download_to_file_complete_callback)
        .Run(
            /* network_error= */ 0,
            /* content_size= */ kCrxContentLength);
    return base::DoNothing();
  }
};

template <typename T>
class MockNetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  MockNetworkFetcherFactory() = default;
  MockNetworkFetcherFactory(const MockNetworkFetcherFactory&) = delete;
  MockNetworkFetcherFactory& operator=(const MockNetworkFetcherFactory&) =
      delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override {
    return std::make_unique<T>();
  }

 protected:
  ~MockNetworkFetcherFactory() override = default;
};

class MockConfigurator : public AwComponentUpdaterConfigurator {
 public:
  explicit MockConfigurator(PrefService* pref_service,
                            scoped_refptr<update_client::NetworkFetcherFactory>
                                network_fetcher_factory)
      : AwComponentUpdaterConfigurator(base::CommandLine::ForCurrentProcess(),
                                       pref_service),
        network_fetcher_factory_(std::move(network_fetcher_factory)) {}

  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override {
    return network_fetcher_factory_;
  }

  // Disable CUP signing so we can inject the fake CRX.
  bool EnabledCupSigning() const override { return false; }

 protected:
  ~MockConfigurator() override = default;

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
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
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(0);
  }

  void OnCustomUninstall() override { FAIL(); }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override {
    version_ = version;
    install_dir_ = install_dir;
    manifest_ = std::move(manifest);
  }

  bool VerifyInstallation(const base::Value::Dict& manifest,
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

  bool IsComponentReadyInvoked() { return !!manifest_; }
  base::Value::Dict& GetManifest() { return *manifest_; }
  base::FilePath GetInstallDir() const { return install_dir_; }
  base::Version GetVersion() const { return version_; }

 private:
  std::optional<base::Value::Dict> manifest_;
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
  raw_ptr<MockInstallerPolicy> mock_policy_;
};

class AwComponentUpdateServiceTest : public testing::Test {
 public:
  AwComponentUpdateServiceTest() = default;
  ~AwComponentUpdateServiceTest() override = default;

  AwComponentUpdateServiceTest(const AwComponentUpdateServiceTest&) = delete;
  AwComponentUpdateServiceTest& operator=(const AwComponentUpdateServiceTest&) =
      delete;

  static void SetUpTestSuite() {
    RegisterPathProvider();
    component_updater::RegisterPathProvider(
        /*components_system_root_key=*/android_webview::DIR_COMPONENTS_ROOT,
        /*components_system_root_key_alt=*/android_webview::DIR_COMPONENTS_ROOT,
        /*components_user_root_key=*/android_webview::DIR_COMPONENTS_ROOT);
  }

  // Override from testing::Test
  void SetUp() override {
    update_client::RegisterPrefs(test_pref_->registry());

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
  std::unique_ptr<TestingPrefServiceSimple> test_pref_ =
      std::make_unique<TestingPrefServiceSimple>();

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AwComponentUpdateServiceTest, TestComponentReadyWhenOffline) {
  CreateTestFiles(component_install_dir_.AppendASCII(kTestVersion));

  base::RunLoop run_loop;
  TestAwComponentUpdateService service(base::MakeRefCounted<MockConfigurator>(
      test_pref_.get(),
      base::MakeRefCounted<
          MockNetworkFetcherFactory<FailingNetworkFetcher>>()));

  base::OnceClosure closure = run_loop.QuitClosure();
  service.StartComponentUpdateService(
      base::BindOnce(
          [](base::OnceClosure closure, int) { std::move(closure).Run(); },
          std::move(closure)),
      false);
  run_loop.Run();

  ASSERT_TRUE(service.GetMockPolicy()->IsComponentReadyInvoked());
  EXPECT_EQ(service.GetMockPolicy()->GetVersion().GetString(), kTestVersion);
  EXPECT_EQ(service.GetMockPolicy()->GetInstallDir(),
            component_install_dir_.AppendASCII(kTestVersion));
}

TEST_F(AwComponentUpdateServiceTest, TestFreshDownloadingFakeApk) {
  base::RunLoop run_loop;
  TestAwComponentUpdateService service(base::MakeRefCounted<MockConfigurator>(
      test_pref_.get(),
      base::MakeRefCounted<
          MockNetworkFetcherFactory<FakeCrxNetworkFetcher>>()));

  base::OnceClosure closure = run_loop.QuitClosure();
  service.StartComponentUpdateService(
      base::BindOnce(
          [](base::OnceClosure closure, int) { std::move(closure).Run(); },
          std::move(closure)),
      false);
  run_loop.Run();

  ASSERT_TRUE(service.GetMockPolicy()->IsComponentReadyInvoked());
  EXPECT_EQ(service.GetMockPolicy()->GetVersion().GetString(), kTestVersion);
  EXPECT_EQ(service.GetMockPolicy()->GetInstallDir(),
            component_install_dir_.AppendASCII(kTestVersion));

  // Assert that the manifest is valid by asserting a field in it other than
  // version.
  std::string* minimum_chrome_version =
      service.GetMockPolicy()->GetManifest().FindString(
          "minimum_chrome_version");
  ASSERT_TRUE(minimum_chrome_version);
  EXPECT_EQ(*minimum_chrome_version, "50");
}

TEST_F(AwComponentUpdateServiceTest, TestOnDemandUpdateRequest) {
  CreateTestFiles(component_install_dir_.AppendASCII(kTestVersion));
  base::RunLoop run_loop;
  TestAwComponentUpdateService service(base::MakeRefCounted<MockConfigurator>(
      test_pref_.get(),
      base::MakeRefCounted<
          MockNetworkFetcherFactory<OnDemandNetworkFetcher>>()));
  base::OnceClosure closure = run_loop.QuitClosure();
  service.StartComponentUpdateService(
      base::BindOnce(
          [](base::OnceClosure closure, int) { std::move(closure).Run(); },
          std::move(closure)),
      true);
  run_loop.Run();

  ASSERT_TRUE(service.GetMockPolicy()->IsComponentReadyInvoked());
  EXPECT_EQ(service.GetMockPolicy()->GetVersion().GetString(), kTestVersion);
  EXPECT_EQ(service.GetMockPolicy()->GetInstallDir(),
            component_install_dir_.AppendASCII(kTestVersion));
}

}  // namespace android_webview
