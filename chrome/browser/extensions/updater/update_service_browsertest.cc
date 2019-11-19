// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/content_verifier_test_utils.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/chrome_update_client_config.h"
#include "chrome/browser/extensions/updater/extension_update_client_base_browsertest.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"

namespace extensions {

namespace {

const char kExtensionId[] = "aohghmighlieiainnegkcijnfilokake";

using UpdateClientEvents = update_client::UpdateClient::Observer::Events;

}  // namespace

class UpdateServiceTest : public ExtensionUpdateClientBaseTest {
 public:
  UpdateServiceTest() : ExtensionUpdateClientBaseTest() {}
  ~UpdateServiceTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionUpdateClientBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
  }

  bool ShouldEnableContentVerification() override { return true; }
};

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, NoUpdate) {
  // Verify that UpdateService runs correctly when there's no update.
  base::ScopedAllowBlockingForTesting allow_io;
  base::HistogramTester histogram_tester;

  // Mock a no-update response.
  ASSERT_TRUE(update_interceptor_->ExpectRequest(
      std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
      test_data_dir_.AppendASCII("updater/updatecheck_reply_noupdate_1.json")));

  const base::FilePath crx_path = test_data_dir_.AppendASCII("updater/v1.crx");
  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);
  EXPECT_EQ(kExtensionId, extension->id());

  extensions::ExtensionUpdater::CheckParams params;
  params.ids = {kExtensionId};
  extension_service()->updater()->CheckNow(std::move(params));

  // UpdateService should emit a not-updated event.
  EXPECT_EQ(UpdateClientEvents::COMPONENT_NOT_UPDATED,
            WaitOnComponentUpdaterCompleteEvent(kExtensionId));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Extensions.ExtensionUpdaterUpdateResults"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(ExtensionUpdaterUpdateResult::NO_UPDATE), 1)));
  histogram_tester.ExpectTotalCount(
      "Extensions.UnifiedExtensionUpdaterUpdateCheckErrors", 0);

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();

  // No update, thus no download nor ping activities.
  EXPECT_EQ(0, get_interceptor_count());
  EXPECT_EQ(0, ping_interceptor_->GetCount())
      << ping_interceptor_->GetRequestsAsString();

  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(kExtensionId, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.10", app.FindKey("version")->GetString());
    EXPECT_TRUE(app.FindKey("enabled")->GetBool());
}

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, UpdateCheckError) {
  // Verify that UpdateService works correctly when there's an error in the
  // update check phase.
  base::ScopedAllowBlockingForTesting allow_io;
  base::HistogramTester histogram_tester;

  // Mock an update check error.
    ASSERT_TRUE(update_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
        net::HTTP_FORBIDDEN));

  const base::FilePath crx_path = test_data_dir_.AppendASCII("updater/v1.crx");
  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);
  EXPECT_EQ(kExtensionId, extension->id());

  extensions::ExtensionUpdater::CheckParams params;
  params.ids = {kExtensionId};
  extension_service()->updater()->CheckNow(std::move(params));

  // UpdateService should emit an error update event.
  EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATE_ERROR,
            WaitOnComponentUpdaterCompleteEvent(kExtensionId));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Extensions.ExtensionUpdaterUpdateResults"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR),
          1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Extensions.UnifiedExtensionUpdaterUpdateCheckErrors"),
              testing::ElementsAre(base::Bucket(403, 1)));

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();

  // Error, thus no download nor ping activities.
  EXPECT_EQ(0, get_interceptor_count());
  EXPECT_EQ(0, ping_interceptor_->GetCount())
      << ping_interceptor_->GetRequestsAsString();

  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(kExtensionId, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.10", app.FindKey("version")->GetString());
    EXPECT_TRUE(app.FindKey("enabled")->GetBool());
}

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, TwoUpdateCheckErrors) {
  // Verify that the UMA counters are emitted properly when there are 2 update
  // checks with different number of extensions, both of which result in errors.
  base::ScopedAllowBlockingForTesting allow_io;
  base::HistogramTester histogram_tester;

  // Mock update check errors.
    ASSERT_TRUE(update_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
        net::HTTP_NOT_MODIFIED));
    ASSERT_TRUE(update_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
        net::HTTP_USE_PROXY));

  const base::FilePath crx_path1 = test_data_dir_.AppendASCII("updater/v1.crx");
  const base::FilePath crx_path2 = test_data_dir_.AppendASCII("updater/v2.crx");
  const Extension* extension1 =
      InstallExtension(crx_path1, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  const Extension* extension2 =
      InstallExtension(crx_path2, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension1 && extension2);

  extensions::ExtensionUpdater::CheckParams params;

  base::RunLoop run_loop1;
  params.ids = {extension1->id(), extension2->id()};
  params.callback = run_loop1.QuitClosure();
  extension_service()->updater()->CheckNow(std::move(params));
  run_loop1.Run();

  base::RunLoop run_loop2;
  params.ids = {extension1->id()};
  params.callback = run_loop2.QuitClosure();
  extension_service()->updater()->CheckNow(std::move(params));
  run_loop2.Run();

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Extensions.ExtensionUpdaterUpdateResults"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR),
          3)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Extensions.UnifiedExtensionUpdaterUpdateCheckErrors"),
              testing::ElementsAre(base::Bucket(304, 2), base::Bucket(305, 1)));

  ASSERT_EQ(2, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();

  // Error, thus no download nor ping activities.
  EXPECT_EQ(0, get_interceptor_count());
  EXPECT_EQ(0, ping_interceptor_->GetCount())
      << ping_interceptor_->GetRequestsAsString();
}

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, SuccessfulUpdate) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::HistogramTester histogram_tester;

  // Mock an update response.
    const base::FilePath update_response =
        test_data_dir_.AppendASCII("updater/updatecheck_reply_update_1.json");
    const base::FilePath ping_response =
        test_data_dir_.AppendASCII("updater/ping_reply_1.json");
    ASSERT_TRUE(update_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
        update_response));
    ASSERT_TRUE(ping_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
        ping_response));

  const base::FilePath crx_path = test_data_dir_.AppendASCII("updater/v1.crx");
  set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() != "/download/v1.crx")
          return false;

        content::URLLoaderInterceptor::WriteResponse(crx_path,
                                                     params->client.get());
        return true;
      }));

  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);
  EXPECT_EQ(kExtensionId, extension->id());

  base::RunLoop run_loop;

  extensions::ExtensionUpdater::CheckParams params;
  params.ids = {kExtensionId};
  params.callback = run_loop.QuitClosure();
  extension_service()->updater()->CheckNow(std::move(params));

  EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATED,
            WaitOnComponentUpdaterCompleteEvent(kExtensionId));

  run_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Extensions.ExtensionUpdaterUpdateResults"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(ExtensionUpdaterUpdateResult::UPDATE_SUCCESS), 1)));
  histogram_tester.ExpectTotalCount(
      "Extensions.UnifiedExtensionUpdaterUpdateCheckErrors", 0);

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, get_interceptor_count());

  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(kExtensionId, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.10", app.FindKey("version")->GetString());
    EXPECT_TRUE(app.FindKey("enabled")->GetBool());
}

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, PolicyCorrupted) {
  base::ScopedAllowBlockingForTesting allow_io;

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = extension_service();

    const base::FilePath update_response =
        test_data_dir_.AppendASCII("updater/updatecheck_reply_update_1.json");
    const base::FilePath ping_response =
        test_data_dir_.AppendASCII("updater/ping_reply_1.json");
    ASSERT_TRUE(update_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
        update_response));
    ASSERT_TRUE(ping_interceptor_->ExpectRequest(
        std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
        ping_response));

  const base::FilePath crx_path = test_data_dir_.AppendASCII("updater/v1.crx");
  set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() != "/download/v1.crx")
          return false;

        content::URLLoaderInterceptor::WriteResponse(crx_path,
                                                     params->client.get());
        return true;
      }));

  // Setup fake policy and update check objects.
  content_verifier_test::ForceInstallProvider policy(kExtensionId);
  system->management_policy()->RegisterProvider(&policy);
  auto external_provider = std::make_unique<MockExternalProvider>(
      service, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  external_provider->UpdateOrAddExtension(
      std::make_unique<ExternalInstallInfoUpdateUrl>(
          kExtensionId, std::string() /* install_parameter */,
          extension_urls::GetWebstoreUpdateUrl(),
          Manifest::EXTERNAL_POLICY_DOWNLOAD, 0 /* creation_flags */,
          true /* mark_acknowledged */));
  service->AddProviderForTesting(std::move(external_provider));

  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);
  EXPECT_EQ(kExtensionId, extension->id());

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), kExtensionId);
  ContentVerifier* verifier = system->content_verifier();
  verifier->VerifyFailedForTest(kExtensionId, ContentVerifyJob::HASH_MISMATCH);

  // Make sure the extension first got disabled due to corruption.
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(kExtensionId);
  EXPECT_TRUE(reasons & disable_reason::DISABLE_CORRUPTED);

  // Make sure the extension then got re-installed, and that after reinstall it
  // is no longer disabled due to corruption.
  EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATED,
            WaitOnComponentUpdaterCompleteEvent(kExtensionId));

  reasons = prefs->GetDisableReasons(kExtensionId);
  EXPECT_FALSE(reasons & disable_reason::DISABLE_CORRUPTED);

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, get_interceptor_count());

  // Make sure that the update check request is formed correctly when the
  // extension is corrupted:
  // - version="0.0.0.0"
  // - installsource="reinstall"
  // - installedby="policy"
  // - enabled="0"
  // - <disabled reason="1024"/>
  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(kExtensionId, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.0.0.0", app.FindKey("version")->GetString());
    EXPECT_EQ("reinstall", app.FindKey("installsource")->GetString());
    EXPECT_EQ("policy", app.FindKey("installedby")->GetString());
    EXPECT_FALSE(app.FindKey("enabled")->GetBool());
    const auto& disabled = app.FindKey("disabled")->GetList()[0];
    EXPECT_EQ(disable_reason::DISABLE_CORRUPTED,
              disabled.FindKey("reason")->GetInt());
}

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, UninstallExtensionWhileUpdating) {
  // This test is to verify that the extension updater engine (update client)
  // works correctly when an extension is uninstalled when the extension updater
  // is in progress.
  base::ScopedAllowBlockingForTesting allow_io;

  const base::FilePath crx_path = test_data_dir_.AppendASCII("updater/v1.crx");

  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);
  EXPECT_EQ(kExtensionId, extension->id());

  base::RunLoop run_loop;

  extensions::ExtensionUpdater::CheckParams params;
  params.ids = {kExtensionId};
  params.callback = run_loop.QuitClosure();
  extension_service()->updater()->CheckNow(std::move(params));

  // Uninstall the extension right before the message loop is executed to
  // emulate uninstalling an extension in the middle of an extension update.
  extension_service()->UninstallExtension(
      kExtensionId, extensions::UNINSTALL_REASON_COMPONENT_REMOVED, nullptr);

  // Update client should issue an update error event for this extension.
  ASSERT_EQ(UpdateClientEvents::COMPONENT_UPDATE_ERROR,
            WaitOnComponentUpdaterCompleteEvent(kExtensionId));

  run_loop.Run();

  EXPECT_EQ(0, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(0, get_interceptor_count());
}

class PolicyUpdateServiceTest : public ExtensionUpdateClientBaseTest,
                                public testing::WithParamInterface<bool> {
 public:
  PolicyUpdateServiceTest() : ExtensionUpdateClientBaseTest() {}
  ~PolicyUpdateServiceTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionUpdateClientBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionUpdateClientBaseTest::SetUpInProcessBrowserTestFixture();

    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    // ExtensionManagementPolicyUpdater requires a single-threaded context to
    // call RunLoop::RunUntilIdle internally, and it isn't ready at this setup
    // moment.
    base::test::TaskEnvironment env;
    ExtensionManagementPolicyUpdater management_policy(&policy_provider_);
    management_policy.SetIndividualExtensionAutoInstalled(
        id_, extension_urls::kChromeWebstoreUpdateURL, true /* forced */);

    // The policy will force the new install of an extension, which the
    // component updater doesn't support yet. We still rely on the old updater
    // to install a new extension.
    const base::FilePath crx_path =
        test_data_dir_.AppendASCII("updater/v1.crx");
    ExtensionDownloader::set_test_delegate(&downloader_);
    downloader_.AddResponse(id_, "2", crx_path);
  }

  void SetUpNetworkInterceptors() override {
    ExtensionUpdateClientBaseTest::SetUpNetworkInterceptors();

    const base::FilePath crx_path =
        test_data_dir_.AppendASCII("updater/v1.crx");
    set_interceptor_hook(base::BindLambdaForTesting(
        [=](content::URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url.path() != "/download/v1.crx")
            return false;

          content::URLLoaderInterceptor::WriteResponse(crx_path,
                                                       params->client.get());
          return true;
        }));
      const base::FilePath update_response =
          test_data_dir_.AppendASCII("updater/updatecheck_reply_update_1.json");
      const base::FilePath ping_response =
          test_data_dir_.AppendASCII("updater/ping_reply_1.json");

      ASSERT_TRUE(update_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
          update_response));
      ASSERT_TRUE(update_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
          update_response));
      ASSERT_TRUE(update_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
          update_response));
      ASSERT_TRUE(update_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("updatecheck":{)"),
          update_response));
      ASSERT_TRUE(ping_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
          ping_response));
      ASSERT_TRUE(ping_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
          ping_response));
      ASSERT_TRUE(ping_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
          ping_response));
      ASSERT_TRUE(ping_interceptor_->ExpectRequest(
          std::make_unique<update_client::PartialMatch>(R"("eventtype":)"),
          ping_response));
  }

  std::vector<GURL> GetUpdateUrls() const override {
    return {
        https_server_for_update_.GetURL("/policy-updatehost/service/update")};
  }

  std::vector<GURL> GetPingUrls() const override {
    return {https_server_for_ping_.GetURL("/policy-pinghost/service/ping")};
  }

 protected:
  // The id of the extension we want to have force-installed.
  std::string id_ = "aohghmighlieiainnegkcijnfilokake";

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
  content_verifier_test::DownloaderTestDelegate downloader_;
};

// Tests that if CheckForExternalUpdates() fails, then we retry reinstalling
// corrupted policy extensions. For example: if network is unavailable,
// CheckForExternalUpdates() will fail.
IN_PROC_BROWSER_TEST_F(PolicyUpdateServiceTest, FailedUpdateRetries) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionService* service = extension_service();
  ContentVerifier* verifier =
      ExtensionSystem::Get(profile())->content_verifier();

  // Wait for the extension to be installed by the policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  }

  content_verifier_test::DelayTracker delay_tracker;
  service->set_external_updates_disabled_for_test(true);
  TestExtensionRegistryObserver registry_observer(registry, id_);
  verifier->VerifyFailedForTest(id_, ContentVerifyJob::HASH_MISMATCH);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  const std::vector<base::TimeDelta>& calls = delay_tracker.calls();
  ASSERT_EQ(1u, calls.size());
  EXPECT_EQ(base::TimeDelta(), delay_tracker.calls()[0]);

  delay_tracker.Proceed();

  // Remove the override and set ExtensionService to update again. The extension
  // should be now installed.
  service->set_external_updates_disabled_for_test(false);
  delay_tracker.StopWatching();
  delay_tracker.Proceed();

  EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATED,
            WaitOnComponentUpdaterCompleteEvent(id_));

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, get_interceptor_count());

  // Make sure that the update check request is formed correctly when the
  // extension is corrupted:
  // - version="0.0.0.0"
  // - installsource="reinstall"
  // - installedby="policy"
  // - enabled="0"
  // - <disabled reason="1024"/>
  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(id_, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.0.0.0", app.FindKey("version")->GetString());
    EXPECT_EQ("reinstall", app.FindKey("installsource")->GetString());
    EXPECT_EQ("policy", app.FindKey("installedby")->GetString());
    EXPECT_FALSE(app.FindKey("enabled")->GetBool());
    const auto& disabled = app.FindKey("disabled")->GetList()[0];
    EXPECT_EQ(disable_reason::DISABLE_CORRUPTED,
              disabled.FindKey("reason")->GetInt());
}

IN_PROC_BROWSER_TEST_F(PolicyUpdateServiceTest, Backoff) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ContentVerifier* verifier =
      ExtensionSystem::Get(profile())->content_verifier();

  // Wait for the extension to be installed by the policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  }

  // Setup to intercept reinstall action, so we can see what the delay would
  // have been for the real action.
  content_verifier_test::DelayTracker delay_tracker;

  // Do 4 iterations of disabling followed by reinstall.
  const size_t iterations = 4;
  for (size_t i = 0; i < iterations; i++) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    verifier->VerifyFailedForTest(id_, ContentVerifyJob::HASH_MISMATCH);
    EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
    // Resolve the request to |delay_tracker|, so the reinstallation can
    // proceed.
    delay_tracker.Proceed();
    EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATED,
              WaitOnComponentUpdaterCompleteEvent(id_));
  }

  ASSERT_EQ(4, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(4, get_interceptor_count());

  const std::vector<base::TimeDelta>& calls = delay_tracker.calls();

  // After |delay_tracker| resolves the 4 (|iterations|) reinstallation
  // requests, it will get an additional request (right away) for retrying
  // reinstallation.
  // Note: the additional request in non-test environment will arrive with
  // a (backoff) delay. But during test, |delay_tracker| issues the request
  // immediately.
  ASSERT_EQ(iterations, calls.size() - 1);
  // Assert that the first reinstall action happened with a delay of 0, and
  // then kept growing each additional time.
  EXPECT_EQ(base::TimeDelta(), delay_tracker.calls()[0]);
  for (size_t i = 1; i < delay_tracker.calls().size(); i++) {
    EXPECT_LT(calls[i - 1], calls[i]);
  }
}

// We want to test what happens at startup with a corroption-disabled policy
// force installed extension. So we set that up in the PRE test here.
IN_PROC_BROWSER_TEST_F(PolicyUpdateServiceTest, PRE_PolicyCorruptedOnStartup) {
  // This is to not allow any corrupted resintall to proceed.
  content_verifier_test::DelayTracker delay_tracker;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  TestExtensionRegistryObserver registry_observer(registry, id_);

  // Wait for the extension to be installed by policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_))
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());

  // Simulate corruption of the extension so that we can test what happens
  // at startup in the non-PRE test.
  ContentVerifier* verifier =
      ExtensionSystem::Get(profile())->content_verifier();
  verifier->VerifyFailedForTest(id_, ContentVerifyJob::HASH_MISMATCH);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(id_);
  EXPECT_TRUE(reasons & disable_reason::DISABLE_CORRUPTED);
  EXPECT_EQ(1u, delay_tracker.calls().size());

  EXPECT_EQ(0, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(0, get_interceptor_count());
}

// Now actually test what happens on the next startup after the PRE test above.
IN_PROC_BROWSER_TEST_F(PolicyUpdateServiceTest, PolicyCorruptedOnStartup) {
  // Depdending on timing, the extension may have already been reinstalled
  // between SetUpInProcessBrowserTestFixture and now (usually not during local
  // testing on a developer machine, but sometimes on a heavily loaded system
  // such as the build waterfall / trybots). If the reinstall didn't already
  // happen, wait for it.

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  int disable_reasons = prefs->GetDisableReasons(id_);
  if (disable_reasons & disable_reason::DISABLE_CORRUPTED) {
    EXPECT_EQ(UpdateClientEvents::COMPONENT_UPDATED,
              WaitOnComponentUpdaterCompleteEvent(id_));
    disable_reasons = prefs->GetDisableReasons(id_);
  }

  EXPECT_FALSE(disable_reasons & disable_reason::DISABLE_CORRUPTED);
  EXPECT_TRUE(registry->enabled_extensions().Contains(id_));

  ASSERT_EQ(1, update_interceptor_->GetCount())
      << update_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, get_interceptor_count());

  const std::string update_request =
      std::get<0>(update_interceptor_->GetRequests()[0]);
    const auto root = base::JSONReader::Read(update_request);
    ASSERT_TRUE(root);
    const auto& app = root->FindKey("request")->FindKey("app")->GetList()[0];
    EXPECT_EQ(id_, app.FindKey("appid")->GetString());
    EXPECT_EQ("0.0.0.0", app.FindKey("version")->GetString());
    EXPECT_EQ("reinstall", app.FindKey("installsource")->GetString());
    EXPECT_EQ("policy", app.FindKey("installedby")->GetString());
    EXPECT_FALSE(app.FindKey("enabled")->GetBool());
    const auto& disabled = app.FindKey("disabled")->GetList()[0];
    EXPECT_EQ(disable_reason::DISABLE_CORRUPTED,
              disabled.FindKey("reason")->GetInt());
}

}  // namespace extensions
