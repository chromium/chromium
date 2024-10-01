// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/startup_app_launcher.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_helper.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/ash/extensions/test_external_cache.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom-forward.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom-shared.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/sync/model/string_ordinal.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "test_kiosk_extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using extensions::Extension;

namespace ash {

namespace {
using ::extensions::ExternalInstallInfoFile;
using ::extensions::ExternalInstallInfoUpdateUrl;
using ::extensions::Manifest;
using ::extensions::mojom::ManifestLocation;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

constexpr char kTestPrimaryAppId[] = "abcdefghabcdefghabcdefghabcdefgh";

constexpr char kSecondaryAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";

constexpr char kExtraSecondaryAppId[] = "aaaaccccaaaaccccaaaaccccaaaacccc";

constexpr char kTestUserAccount[] = "user@test";

constexpr char kCwsUrl[] = "http://cws/";

enum class LaunchState {
  kNotStarted,
  kInitializingNetwork,
  kInstallingApp,
  kReadyToLaunch,
  kLaunchSucceeded,
  kLaunchFailed
};

class TestAppLaunchDelegate : public KioskAppLauncher::NetworkDelegate,
                              public KioskAppLauncher::Observer {
 public:
  TestAppLaunchDelegate() = default;
  TestAppLaunchDelegate(const TestAppLaunchDelegate&) = delete;
  TestAppLaunchDelegate& operator=(const TestAppLaunchDelegate&) = delete;
  ~TestAppLaunchDelegate() override = default;

  KioskAppLaunchError::Error launch_error() const { return launch_error_; }

  void set_network_ready(bool network_ready) { network_ready_ = network_ready; }

  void ClearLaunchStateChanges() {
    while (!launch_state_changes_.IsEmpty()) {
      launch_state_changes_.Take();
    }
  }

  LaunchState WaitForNextLaunchState() { return launch_state_changes_.Take(); }

  bool ExpectNoLaunchStateChanges() {
    // Wait a bit to give the state changes a chance to arrive
    base::RunLoop().RunUntilIdle();
    return launch_state_changes_.IsEmpty();
  }

  // `KioskAppLauncher::NetworkDelegate`:
  void InitializeNetwork() override {
    SetLaunchState(LaunchState::kInitializingNetwork);
  }
  bool IsNetworkReady() const override { return network_ready_; }

  // `KioskAppLauncher::Observer`:
  void OnAppInstalling() override {
    SetLaunchState(LaunchState::kInstallingApp);
  }
  void OnAppPrepared() override { SetLaunchState(LaunchState::kReadyToLaunch); }
  void OnAppLaunched() override {
    SetLaunchState(LaunchState::kLaunchSucceeded);
  }
  void OnLaunchFailed(KioskAppLaunchError::Error error) override {
    launch_error_ = error;
    SetLaunchState(LaunchState::kLaunchFailed);
  }

 private:
  void SetLaunchState(LaunchState state) {
    launch_state_changes_.AddValue(state);
  }

  KioskAppLaunchError::Error launch_error_ = KioskAppLaunchError::Error::kNone;

  bool network_ready_ = false;

  base::test::RepeatingTestFuture<LaunchState> launch_state_changes_;
};

class AppLaunchTracker : public extensions::TestEventRouter::EventObserver {
 public:
  AppLaunchTracker(const std::string& app_id,
                   extensions::TestEventRouter* event_router)
      : app_id_(app_id), event_router_(event_router) {
    event_router->AddEventObserver(this);
  }
  AppLaunchTracker(const AppLaunchTracker&) = delete;
  AppLaunchTracker& operator=(const AppLaunchTracker&) = delete;
  ~AppLaunchTracker() override { event_router_->RemoveEventObserver(this); }

  int kiosk_launch_count() const { return kiosk_launch_count_; }

  // TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    ADD_FAILURE() << "Unexpected broadcast " << event.event_name;
  }

  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    ASSERT_EQ(extension_id, app_id_);

    ASSERT_EQ(event.event_name,
              extensions::api::app_runtime::OnLaunched::kEventName);
    ASSERT_EQ(1u, event.event_args.size());

    const base::Value& launch_data = event.event_args[0];
    std::optional<bool> is_kiosk_session =
        launch_data.GetDict().FindBool("isKioskSession");
    ASSERT_TRUE(is_kiosk_session);
    EXPECT_TRUE(*is_kiosk_session);
    ++kiosk_launch_count_;
  }

 private:
  const std::string app_id_;
  raw_ptr<extensions::TestEventRouter> event_router_;
  int kiosk_launch_count_ = 0;
};

// Simulates extension service behavior related to external extensions loading,
// but does not initiate found extension's CRX installation - instead, it keeps
// track of pending extension installations, and expect the test code to finish
// the pending extension installations.
class TestKioskLoaderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  TestKioskLoaderVisitor(content::BrowserContext* browser_context,
                         extensions::ExtensionRegistry* extension_registry,
                         extensions::ExtensionService* extension_service)
      : browser_context_(browser_context),
        extension_registry_(extension_registry),
        extension_service_(extension_service) {}
  TestKioskLoaderVisitor(const TestKioskLoaderVisitor&) = delete;
  TestKioskLoaderVisitor& operator=(const TestKioskLoaderVisitor&) = delete;
  ~TestKioskLoaderVisitor() override = default;

  const std::set<std::string>& pending_crx_files() const {
    return pending_crx_files_;
  }
  const std::set<std::string>& pending_update_urls() const {
    return pending_update_urls_;
  }

  bool FinishPendingInstall(const Extension* extension) {
    if (!pending_crx_files_.count(extension->id()) &&
        !pending_update_urls_.count(extension->id())) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()->IsIdPending(
            extension->id())) {
      return false;
    }

    pending_crx_files_.erase(extension->id());
    pending_update_urls_.erase(extension->id());
    extension_service_->OnExtensionInstalled(
        extension, syncer::StringOrdinal::CreateInitialOrdinal(),
        extensions::kInstallFlagInstallImmediately);
    auto installer = extensions::CrxInstaller::CreateSilent(extension_service_);
    extensions::InstallTracker::Get(browser_context_)
        ->OnFinishCrxInstall(*installer, extension->id(), true);
    return true;
  }

  bool FailPendingInstall(const std::string& extension_id) {
    if (!pending_crx_files_.count(extension_id) &&
        !pending_update_urls_.count(extension_id)) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()->IsIdPending(
            extension_id)) {
      return false;
    }

    pending_crx_files_.erase(extension_id);
    pending_update_urls_.erase(extension_id);
    auto installer = extensions::CrxInstaller::CreateSilent(extension_service_);
    extensions::InstallTracker::Get(browser_context_)
        ->OnFinishCrxInstall(*installer, extension_id, false);
    extension_service_->pending_extension_manager()->Remove(extension_id);
    return true;
  }

  // extensions::ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    const extensions::Extension* existing =
        extension_registry_->GetExtensionById(
            info.extension_id, extensions::ExtensionRegistry::EVERYTHING);
    // Already exists, and does not require update.
    if (existing && existing->version().CompareTo(info.version) >= 0) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()->AddFromExternalFile(
            info.extension_id, info.crx_location, info.version,
            info.creation_flags, info.mark_acknowledged)) {
      return false;
    }

    pending_crx_files_.insert(info.extension_id);
    auto installer = extensions::CrxInstaller::CreateSilent(extension_service_);
    extensions::InstallTracker::Get(browser_context_)
        ->OnBeginCrxInstall(*installer, info.extension_id);
    return true;
  }
  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    if (extension_registry_->GetExtensionById(
            info.extension_id, extensions::ExtensionRegistry::EVERYTHING)) {
      return false;
    }

    if (!extension_service_->pending_extension_manager()
             ->AddFromExternalUpdateUrl(
                 info.extension_id, info.install_parameter, info.update_url,
                 info.download_location, info.creation_flags,
                 info.mark_acknowledged)) {
      return false;
    }

    pending_update_urls_.insert(info.extension_id);
    auto installer = extensions::CrxInstaller::CreateSilent(extension_service_);
    extensions::InstallTracker::Get(browser_context_)
        ->OnBeginCrxInstall(*installer, info.extension_id);
    return true;
  }
  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {}
  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    for (const auto& extension : update_url_extensions) {
      OnExternalExtensionUpdateUrlFound(extension, false);
    }

    for (const auto& extension : file_extensions) {
      OnExternalExtensionFileFound(extension);
    }

    for (const auto& extension_id : removed_extensions) {
      extension_service_->UninstallExtension(
          extension_id,
          extensions::UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, nullptr);
    }
  }

 private:
  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<extensions::ExtensionService> extension_service_;

  std::set<std::string> pending_crx_files_;
  std::set<std::string> pending_update_urls_;
};

void InitAppWindow(extensions::AppWindow* app_window, const gfx::Rect& bounds) {
  // Create a TestAppWindowContents for the ShellAppDelegate to initialize the
  // ShellExtensionWebContentsObserver with.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(app_window->browser_context())));
  auto app_window_contents =
      std::make_unique<extensions::TestAppWindowContents>(
          std::move(web_contents));

  // Initialize the web contents and AppWindow.
  app_window->app_delegate()->InitWebContents(
      app_window_contents->GetWebContents());

  content::RenderFrameHost* main_frame =
      app_window_contents->GetWebContents()->GetPrimaryMainFrame();
  DCHECK(main_frame);

  extensions::AppWindow::CreateParams params;
  params.content_spec.bounds = bounds;
  app_window->Init(GURL(), std::move(app_window_contents), main_frame, params);
}

extensions::AppWindow* CreateAppWindow(Profile* profile,
                                       const Extension& app,
                                       gfx::Rect bounds = {}) {
  extensions::AppWindow* app_window = new extensions::AppWindow(
      profile, std::make_unique<ChromeAppDelegate>(profile, true), &app);
  InitAppWindow(app_window, bounds);
  return app_window;
}

// This class overrides some of the behaviour of `KioskChromeAppManager`, which
// is the `KioskAppManagerBase` implementation for ChromeApp kiosk. Notably it
// injects its own `ExternalCache` implementation and overrides the construction
// on an `KioskBrowserSession` object.
class ScopedKioskAppManagerOverrides : public KioskChromeAppManager::Overrides {
 public:
  ScopedKioskAppManagerOverrides() {
    KioskChromeAppManager::InitializeForTesting(this);
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  chromeos::TestExternalCache* external_cache() { return external_cache_; }

  void InitializePrimaryAppState() {
    // Inject test kiosk app data to prevent KioskChromeAppManager from
    // attempting to load it.
    // TODO(tbarzic): Introducing a test KioskAppData class that overrides app
    //     data load logic, and injecting a KioskAppData object factory to
    //     KioskChromeAppManager would be a cleaner solution here.
    KioskChromeAppManager::Get()->AddAppForTest(
        kTestPrimaryAppId, AccountId::FromUserEmail(kTestUserAccount),
        GURL(kCwsUrl),
        /*required_platform_version=*/"");

    accounts_settings_helper_ = std::make_unique<ScopedCrosSettingsTestHelper>(
        /*create_service=*/false);
    accounts_settings_helper_->ReplaceDeviceSettingsProviderWithStub();

    base::Value::Dict account;
    account.Set(kAccountsPrefDeviceLocalAccountsKeyId, kTestUserAccount);
    account.Set(kAccountsPrefDeviceLocalAccountsKeyType,
                static_cast<int>(policy::DeviceLocalAccountType::kKioskApp));
    account.Set(
        kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
        static_cast<int>(policy::DeviceLocalAccount::EphemeralMode::kUnset));
    account.Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                kTestPrimaryAppId);
    base::Value::List accounts;
    accounts.Append(std::move(account));

    accounts_settings_helper_->Set(kAccountsPrefDeviceLocalAccounts,
                                   base::Value(std::move(accounts)));

    // Set auto-launch kiosk
    accounts_settings_helper_->SetString(
        kAccountsPrefDeviceLocalAccountAutoLoginId, kTestUserAccount);
    accounts_settings_helper_->SetInteger(
        kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
  }

  [[nodiscard]] AssertionResult DownloadPrimaryApp(const Extension& app) {
    if (!external_cache_) {
      return AssertionFailure() << "External cache not initialized";
    }

    if (!external_cache_->pending_downloads().count(app.id())) {
      return AssertionFailure() << "Download not pending: " << app.id();
    }

    if (!external_cache_->SimulateExtensionDownloadFinished(
            app.id(), GetExtensionPath(app.id()), app.VersionString(),
            /*is_update=*/false)) {
      return AssertionFailure() << " Finish download attempt failed";
    }

    return AssertionSuccess();
  }

  [[nodiscard]] AssertionResult PrecachePrimaryApp(
      const extensions::Extension& app) {
    if (!external_cache_) {
      return AssertionFailure() << "External cache not initialized";
    }

    base::test::TestFuture<const std::string&, bool> future;
    external_cache_->PutExternalExtension(
        app.id(), base::FilePath(GetExtensionPath(app.id())),
        app.VersionString(), future.GetCallback());

    if (!std::get<1>(future.Get())) {
      return AssertionFailure() << "Precaching extension failed";
    }

    return AssertionSuccess();
  }

  // KioskChromeAppManager::Overrides:
  std::unique_ptr<chromeos::ExternalCache> CreateExternalCache(
      chromeos::ExternalCacheDelegate* delegate,
      bool always_check_updates) override {
    auto cache = std::make_unique<chromeos::TestExternalCache>(
        delegate, always_check_updates);
    external_cache_ = cache.get();
    return cache;
  }

 private:
  // Note: These tests should not actually create files, so the actual returned
  // path is not too important. Still, putting it under the test's temp dir, in
  // case something unexpectedly tries to do file I/O with the file paths
  // returned here.
  std::string GetExtensionPath(const std::string& app_id) {
    return temp_dir_.GetPath()
        .AppendASCII("test_crx_file")
        .AppendASCII(app_id)
        .value();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ScopedCrosSettingsTestHelper> accounts_settings_helper_;

  raw_ptr<chromeos::TestExternalCache, DanglingUntriaged> external_cache_;
};

TestKioskExtensionBuilder PrimaryAppBuilder() {
  return std::move(
      TestKioskExtensionBuilder(extensions::Manifest::TYPE_PLATFORM_APP,
                                kTestPrimaryAppId)
          .set_version("1.0"));
}

TestKioskExtensionBuilder ExtensionBuilder() {
  return TestKioskExtensionBuilder(extensions::Manifest::TYPE_EXTENSION,
                                   kTestPrimaryAppId);
}

TestKioskExtensionBuilder SecondaryAppBuilder(const std::string& id) {
  return TestKioskExtensionBuilder(extensions::Manifest::TYPE_PLATFORM_APP, id);
}

}  // namespace

using crosapi::mojom::AppInstallParamsPtr;
using crosapi::mojom::ChromeKioskInstallResult;
using crosapi::mojom::ChromeKioskLaunchController;
using crosapi::mojom::ChromeKioskLaunchResult;

// Tests without creating `StartupAppLauncher` object.
class StartupAppLauncherNoCreateTest
    : public extensions::ExtensionServiceTestBase {
 public:
  StartupAppLauncherNoCreateTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::REAL_IO_THREAD)) {}

  StartupAppLauncherNoCreateTest(const StartupAppLauncherNoCreateTest&) =
      delete;
  StartupAppLauncherNoCreateTest& operator=(
      const StartupAppLauncherNoCreateTest&) = delete;
  ~StartupAppLauncherNoCreateTest() override = default;

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp();

    UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ::switches::kForceAppMode);
    command_line_.GetProcessCommandLine()->AppendSwitch(::switches::kAppId);

    extensions::ExtensionServiceTestBase::SetUp();

    kiosk_app_manager_overrides_.InitializePrimaryAppState();

    InitializeEmptyExtensionService();
    external_apps_loader_handler_ = std::make_unique<TestKioskLoaderVisitor>(
        browser_context(), registry(), service());
    CreateAndInitializeKioskAppsProviders(external_apps_loader_handler_.get());

    extensions::TestEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter(browser_context());
    app_launch_tracker_ =
        std::make_unique<AppLaunchTracker>(kTestPrimaryAppId, event_router);
  }

  void TearDown() override {
    primary_app_provider_->ServiceShutdown();
    secondary_apps_provider_->ServiceShutdown();
    external_apps_loader_handler_.reset();

    app_launch_tracker_.reset();

    extensions::ExtensionServiceTestBase::TearDown();

    ash_test_helper_.TearDown();
  }

 protected:
  chromeos::TestExternalCache* external_cache() {
    return kiosk_app_manager_overrides_.external_cache();
  }

  ScopedKioskAppManagerOverrides& kiosk_app_manager_overrides() {
    return kiosk_app_manager_overrides_;
  }

  [[nodiscard]] AssertionResult DownloadPrimaryApp(const Extension& app) {
    return kiosk_app_manager_overrides_.DownloadPrimaryApp(app);
  }

  [[nodiscard]] AssertionResult FinishPrimaryAppInstall(const Extension& app) {
    const std::string& id = app.id();
    if (!external_apps_loader_handler_->pending_crx_files().count(id)) {
      return AssertionFailure() << "App install not pending: " << id;
    }

    if (!external_apps_loader_handler_->FinishPendingInstall(&app)) {
      return AssertionFailure() << "Finish install attempt failed: " << id;
    }

    return AssertionSuccess();
  }

  [[nodiscard]] AssertionResult DownloadAndInstallPrimaryApp(
      const Extension& app) {
    AssertionResult download_result =
        kiosk_app_manager_overrides_.DownloadPrimaryApp(app);
    if (!download_result) {
      return download_result;
    }

    AssertionResult install_result = FinishPrimaryAppInstall(app);
    if (!install_result) {
      return install_result;
    }

    return AssertionSuccess();
  }

  [[nodiscard]] AssertionResult FinishSecondaryExtensionInstall(
      const Extension& extension) {
    const std::string& id = extension.id();
    if (!external_apps_loader_handler_->pending_update_urls().count(id)) {
      return AssertionFailure()
             << "Secondary extension install not pending: " << id;
    }

    if (!external_apps_loader_handler_->FinishPendingInstall(&extension)) {
      return AssertionFailure() << "Finish install attempt failed: " << id;
    }

    return AssertionSuccess();
  }

  void CreateAndInitializeKioskAppsProviders(TestKioskLoaderVisitor* visitor) {
    primary_app_provider_ = std::make_unique<extensions::ExternalProviderImpl>(
        visitor,
        base::MakeRefCounted<chromeos::KioskAppExternalLoader>(
            chromeos::KioskAppExternalLoader::AppClass::kPrimary),
        profile(), ManifestLocation::kExternalPolicy,
        ManifestLocation::kInvalidLocation, extensions::Extension::NO_FLAGS);
    InitializeKioskAppsProvider(primary_app_provider_.get());

    secondary_apps_provider_ =
        std::make_unique<extensions::ExternalProviderImpl>(
            visitor,
            base::MakeRefCounted<chromeos::KioskAppExternalLoader>(
                chromeos::KioskAppExternalLoader::AppClass::kSecondary),
            profile(), ManifestLocation::kExternalPref,
            ManifestLocation::kExternalPrefDownload,
            extensions::Extension::NO_FLAGS);
    InitializeKioskAppsProvider(secondary_apps_provider_.get());
  }

  void InitializeKioskAppsProvider(extensions::ExternalProviderImpl* provider) {
    provider->set_auto_acknowledge(true);
    provider->set_install_immediately(true);
    provider->set_allow_updates(true);
    provider->VisitRegisteredExtension();
  }

  auto CreateStartupAppLauncher() {
    return CreateStartupAppLauncherInternal(/*should_skip_install=*/false);
  }

  auto CreateStartupAppLauncherForSessionRestore() {
    return CreateStartupAppLauncherInternal(/*should_skip_install=*/true);
  }

  void PreinstallApp(const Extension& app) { service()->AddExtension(&app); }

  TestAppLaunchDelegate startup_launch_delegate_;

  std::unique_ptr<AppLaunchTracker> app_launch_tracker_;
  std::unique_ptr<TestKioskLoaderVisitor> external_apps_loader_handler_;

 private:
  std::unique_ptr<KioskAppLauncher> CreateStartupAppLauncherInternal(
      bool should_skip_install) {
    std::unique_ptr<KioskAppLauncher> startup_app_launcher =
        std::make_unique<StartupAppLauncher>(profile(), kTestPrimaryAppId,
                                             should_skip_install,
                                             &startup_launch_delegate_);
    startup_app_launcher->AddObserver(&startup_launch_delegate_);
    return startup_app_launcher;
  }

  AshTestHelper ash_test_helper_;
  base::test::ScopedCommandLine command_line_;

  ScopedKioskAppManagerOverrides kiosk_app_manager_overrides_;

  std::unique_ptr<extensions::ExternalProviderImpl> primary_app_provider_;
  std::unique_ptr<extensions::ExternalProviderImpl> secondary_apps_provider_;
};

// Tests that extension download backoff is reduced during Chrome app Kiosk
// launch.
TEST_F(StartupAppLauncherNoCreateTest, ExtensionDownloadBackoffReduced) {
  ASSERT_TRUE(external_cache());
  EXPECT_FALSE(external_cache()->backoff_policy().has_value());

  auto startup_app_launcher = CreateStartupAppLauncher();

  ASSERT_TRUE(external_cache()->backoff_policy().has_value());
  EXPECT_EQ(external_cache()->backoff_policy()->maximum_backoff_ms, 3000);

  startup_app_launcher.reset();
  EXPECT_FALSE(external_cache()->backoff_policy().has_value());
}

TEST_F(StartupAppLauncherNoCreateTest, AppNotKioskEnabledOnSessionRestore) {
  PreinstallApp(*PrimaryAppBuilder().set_kiosk_enabled(false).Build());
  auto startup_app_launcher = CreateStartupAppLauncherForSessionRestore();

  startup_app_launcher->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher->LaunchApp();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchFailed);

  EXPECT_EQ(startup_launch_delegate_.launch_error(),
            KioskAppLaunchError::Error::kUnableToLaunch);
}

// Tests with `StartupAppLauncher` object created.
class StartupAppLauncherTest : public StartupAppLauncherNoCreateTest {
 public:
  // testing::Test:
  void SetUp() override {
    StartupAppLauncherNoCreateTest::SetUp();
    // Some tests depend on AppService, so wait AppService to be ready.
    WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile()));

    startup_app_launcher_ = CreateStartupAppLauncher();
  }

  void TearDown() override {
    startup_app_launcher_.reset();
    StartupAppLauncherNoCreateTest::TearDown();
  }

 protected:
  void InitializeLauncherWithNetworkReady() {
    startup_launch_delegate_.set_network_ready(true);
    startup_app_launcher_->Initialize();
    EXPECT_TRUE(startup_launch_delegate_.ExpectNoLaunchStateChanges());
  }

  std::unique_ptr<KioskAppLauncher> startup_app_launcher_;
};

TEST_F(StartupAppLauncherTest, PrimaryAppLaunchFlow) {
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(external_cache());
  EXPECT_EQ(std::set<std::string>({kTestPrimaryAppId}),
            external_cache()->pending_downloads());

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());

  scoped_refptr<const Extension> primary_app = PrimaryAppBuilder().Build();
  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
}

TEST_F(StartupAppLauncherTest, OfflineLaunchWithPrimaryAppPreInstalled) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().set_version("1.0").Build();
  PreinstallApp(*primary_app);

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  // Primary app cache checks finished after the startup app launcher reports
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(
      DownloadPrimaryApp(*PrimaryAppBuilder().set_version("1.1").Build()));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.ExpectNoLaunchStateChanges());

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
}

TEST_F(StartupAppLauncherTest,
       OfflineLaunchWithPrimaryAppPreInstalled_UpdateFoundAfterLaunch) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().set_version("1.0").Build();
  PreinstallApp(*primary_app);

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));

  // Primary app cache checks finished after the app launch
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(
      DownloadPrimaryApp(*PrimaryAppBuilder().set_version("1.1").Build()));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.ExpectNoLaunchStateChanges());
}

TEST_F(StartupAppLauncherTest, PrimaryAppDownloadFailure) {
  base::HistogramTester histogram;
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(external_cache());
  EXPECT_EQ(std::set<std::string>({kTestPrimaryAppId}),
            external_cache()->pending_downloads());
  ASSERT_TRUE(external_cache()->SimulateExtensionDownloadFailed(
      kTestPrimaryAppId,
      extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_FAILED));

  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchFailed);

  EXPECT_EQ(KioskAppLaunchError::Error::kUnableToDownload,
            startup_launch_delegate_.launch_error());

  histogram.ExpectUniqueSample(
      kKioskPrimaryAppInstallErrorHistogram,
      KioskChromeAppManager::PrimaryAppDownloadResult::kCrxFetchFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(StartupAppLauncherTest, PrimaryAppCrxInstallFailure) {
  InitializeLauncherWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(*PrimaryAppBuilder().Build()));
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_TRUE(
      external_apps_loader_handler_->FailPendingInstall(kTestPrimaryAppId));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchFailed);

  EXPECT_EQ(KioskAppLaunchError::Error::kUnableToInstall,
            startup_launch_delegate_.launch_error());
}

TEST_F(StartupAppLauncherTest, PrimaryAppNotKioskEnabled) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().set_kiosk_enabled(false).Build();
  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchFailed);

  EXPECT_EQ(KioskAppLaunchError::Error::kNotKioskEnabled,
            startup_launch_delegate_.launch_error());
}

TEST_F(StartupAppLauncherTest, PrimaryAppIsExtension) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app = ExtensionBuilder().Build();
  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchFailed);

  EXPECT_EQ(KioskAppLaunchError::Error::kNotKioskEnabled,
            startup_launch_delegate_.launch_error());
}

TEST_F(StartupAppLauncherTest, LaunchWithSecondaryApps) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .AddSecondaryExtension(kSecondaryAppId)
          .AddSecondaryExtensionWithEnabledOnLaunch(kExtraSecondaryAppId, false)
          .Build();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  scoped_refptr<const Extension> secondary_app =
      SecondaryAppBuilder(kSecondaryAppId).set_kiosk_enabled(false).Build();
  ASSERT_TRUE(FinishSecondaryExtensionInstall(*secondary_app));

  scoped_refptr<const Extension> disabled_secondary_app =
      SecondaryAppBuilder(kExtraSecondaryAppId).Build();
  ASSERT_TRUE(FinishSecondaryExtensionInstall(*disabled_secondary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kExtraSecondaryAppId));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, LaunchWithSecondaryExtension) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().AddSecondaryExtension(kSecondaryAppId).Build();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  scoped_refptr<const Extension> secondary_extension =
      SecondaryAppBuilder(kSecondaryAppId).set_kiosk_enabled(false).Build();
  ASSERT_TRUE(FinishSecondaryExtensionInstall(*secondary_extension));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, OfflineWithPrimaryAndSecondaryAppInstalled) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .set_version("1.0")
          .AddSecondaryExtension(kSecondaryAppId)
          .Build();
  PreinstallApp(*primary_app);
  PreinstallApp(
      *SecondaryAppBuilder(kSecondaryAppId).set_kiosk_enabled(false).Build());

  startup_app_launcher_->Initialize();

  // Given that the app is offline enabled and installed, the app should be
  // launched immediately, without waiting for network or checking for updates.
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  // Primary app cache checks finished after the startup app launcher reports
  // it's ready should be ignored - i.e. startup app launcher should not attempt
  // to relaunch the app, nor request the update installation.
  startup_app_launcher_->ContinueWithNetworkReady();
  ASSERT_TRUE(
      DownloadPrimaryApp(*PrimaryAppBuilder().set_version("1.1").Build()));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_TRUE(startup_launch_delegate_.ExpectNoLaunchStateChanges());

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, OfflineInstallPreCachedExtension) {
  scoped_refptr<const Extension> primary_app = PrimaryAppBuilder().Build();

  ASSERT_TRUE(kiosk_app_manager_overrides().PrecachePrimaryApp(*primary_app));

  startup_app_launcher_->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
}

TEST_F(StartupAppLauncherTest,
       OfflineInstallPreCachedExtensionNotOfflineEnabled) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().set_offline_enabled(false).Build();

  ASSERT_TRUE(kiosk_app_manager_overrides().PrecachePrimaryApp(*primary_app));

  startup_app_launcher_->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  // When trying to launch app we should realize that the app is not offline
  // enabled and request a network connection.
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInitializingNetwork);

  startup_launch_delegate_.set_network_ready(true);
  startup_app_launcher_->ContinueWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
}

TEST_F(StartupAppLauncherTest,
       OfflineInstallPreCachedExtensionWithSecondaryApps) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .set_offline_enabled(true)
          .AddSecondaryExtension(kSecondaryAppId)
          .Build();

  scoped_refptr<const Extension> secondary_extension =
      SecondaryAppBuilder(kSecondaryAppId).Build();

  ASSERT_TRUE(kiosk_app_manager_overrides().PrecachePrimaryApp(*primary_app));

  startup_app_launcher_->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  ASSERT_TRUE(
      external_apps_loader_handler_->FailPendingInstall(kSecondaryAppId));

  // After install is complete we should realize that the app needs to install
  // secondary apps, so we need to get network set up
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInitializingNetwork);

  startup_launch_delegate_.set_network_ready(true);
  startup_app_launcher_->ContinueWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishSecondaryExtensionInstall(*secondary_extension));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
}

TEST_F(StartupAppLauncherTest,
       OfflineInstallUncachedExtensionShouldForceNetwork) {
  scoped_refptr<const Extension> primary_app = PrimaryAppBuilder().Build();

  startup_app_launcher_->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInitializingNetwork);

  startup_launch_delegate_.set_network_ready(true);
  startup_app_launcher_->ContinueWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
}

TEST_F(StartupAppLauncherTest, IgnoreSecondaryAppsSecondaryApps) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().AddSecondaryExtension(kSecondaryAppId).Build();

  ASSERT_TRUE(DownloadAndInstallPrimaryApp(*primary_app));

  startup_launch_delegate_.ClearLaunchStateChanges();

  scoped_refptr<const Extension> secondary_extension =
      SecondaryAppBuilder(kSecondaryAppId)
          .set_kiosk_enabled(true)
          .AddSecondaryExtension(kExtraSecondaryAppId)
          .Build();

  ASSERT_TRUE(FinishSecondaryExtensionInstall(*secondary_extension));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();
  CreateAppWindow(profile(), *primary_app);

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kLaunchSucceeded);
  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_FALSE(registry()->GetInstalledExtension(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, SecondaryAppCrxInstallFailureTriggersRetry) {
  InitializeLauncherWithNetworkReady();

  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder().AddSecondaryExtension(kSecondaryAppId).Build();

  ASSERT_TRUE(DownloadAndInstallPrimaryApp(*primary_app));
  startup_launch_delegate_.ClearLaunchStateChanges();

  ASSERT_EQ(std::set<std::string>({kSecondaryAppId}),
            external_apps_loader_handler_->pending_update_urls());
  ASSERT_TRUE(
      external_apps_loader_handler_->FailPendingInstall(kSecondaryAppId));

  // The retry mechanism should trigger a new request to initialize the network
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInitializingNetwork);

  startup_app_launcher_->ContinueWithNetworkReady();

  ASSERT_TRUE(DownloadPrimaryApp(*primary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);

  ASSERT_EQ(std::set<std::string>({kSecondaryAppId}),
            external_apps_loader_handler_->pending_update_urls());
  scoped_refptr<const Extension> secondary_app =
      SecondaryAppBuilder(kSecondaryAppId).set_kiosk_enabled(false).Build();
  ASSERT_TRUE(FinishSecondaryExtensionInstall(*secondary_app));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
}

TEST_F(StartupAppLauncherTest,
       SecondaryAppEnabledOnLaunchOverridesInstalledAppState) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, false)
          .AddSecondaryExtensionWithEnabledOnLaunch(kExtraSecondaryAppId, true)
          .Build();

  // Add the secondary app that should be disabled on startup - make it enabled
  // initially, so the test can verify the app gets disabled regardless of the
  // initial state.
  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());

  // Add the secondary app that should be enabled on startup - make it disabled
  // initially, so the test can verify the app gets enabled regardless of the
  // initial state.
  PreinstallApp(*SecondaryAppBuilder(kExtraSecondaryAppId).Build());
  service()->DisableExtension(kExtraSecondaryAppId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(*primary_app));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest,
       KeepInstalledAppStateWithNoEnabledOnLaunchProperty) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .AddSecondaryExtension(kSecondaryAppId)
          .AddSecondaryExtension(kExtraSecondaryAppId)
          .Build();

  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());

  PreinstallApp(*SecondaryAppBuilder(kExtraSecondaryAppId).Build());
  service()->DisableExtension(kExtraSecondaryAppId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(*primary_app));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
}

TEST_F(StartupAppLauncherTest,
       DoNotEnableSecondayAppsDisabledForNonUserActionReason) {
  scoped_refptr<const Extension> primary_app =
      PrimaryAppBuilder()
          .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, true)
          .Build();

  // Add the secondary app that should be enabled on startup - make it disabled
  // initially, so the test can verify the app gets enabled regardless of the
  // initial state.
  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());
  // Disable the secodnary app for a reason different than user action - that
  // disable reason should not be overriden during the kiosk launch.
  service()->DisableExtension(
      kSecondaryAppId,
      extensions::disable_reason::DISABLE_USER_ACTION |
          extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadAndInstallPrimaryApp(*primary_app));

  EXPECT_TRUE(external_apps_loader_handler_->pending_crx_files().empty());
  EXPECT_TRUE(external_apps_loader_handler_->pending_update_urls().empty());
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();

  EXPECT_EQ(1, app_launch_tracker_->kiosk_launch_count());

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, PrimaryAppUpdatesToDisabledOnLaunch) {
  PreinstallApp(*PrimaryAppBuilder()
                     .AddSecondaryExtension(kSecondaryAppId)
                     .set_version("1.0")
                     .set_offline_enabled(false)
                     .Build());
  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());

  scoped_refptr<const Extension> primary_app_update =
      PrimaryAppBuilder()
          .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, false)
          .set_version("1.1")
          .Build();

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(*primary_app_update));
  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app_update));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            extensions::ExtensionPrefs::Get(browser_context())
                ->GetDisableReasons(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, PrimaryAppUpdatesToEnabledOnLaunch) {
  PreinstallApp(
      *PrimaryAppBuilder()
           .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, false)
           .set_version("1.0")
           .set_offline_enabled(false)
           .Build());
  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());
  service()->DisableExtension(kSecondaryAppId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  scoped_refptr<const Extension> primary_app_update =
      PrimaryAppBuilder()
          .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, true)
          .set_version("1.1")
          .Build();

  InitializeLauncherWithNetworkReady();
  ASSERT_TRUE(DownloadPrimaryApp(*primary_app_update));
  ASSERT_TRUE(FinishPrimaryAppInstall(*primary_app_update));

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kInstallingApp);
  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);
  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
}

TEST_F(StartupAppLauncherTest, SecondaryExtensionStateOnSessionRestore) {
  PreinstallApp(
      *PrimaryAppBuilder()
           .AddSecondaryExtensionWithEnabledOnLaunch(kSecondaryAppId, false)
           .AddSecondaryExtensionWithEnabledOnLaunch(kExtraSecondaryAppId, true)
           .Build());

  // Add the secondary app that should be disabled on launch - make it enabled
  // initially, and let test verify it remains enabled during the launch.
  PreinstallApp(*SecondaryAppBuilder(kSecondaryAppId).Build());

  // Add the secondary app that should be enabled on launch - make it disabled
  // initially, and let test verify the app remains disabled during the launch.
  PreinstallApp(*SecondaryAppBuilder(kExtraSecondaryAppId).Build());
  service()->DisableExtension(kExtraSecondaryAppId,
                              extensions::disable_reason::DISABLE_USER_ACTION);

  startup_app_launcher_ = CreateStartupAppLauncherForSessionRestore();

  startup_launch_delegate_.set_network_ready(true);
  startup_app_launcher_->Initialize();

  EXPECT_EQ(startup_launch_delegate_.WaitForNextLaunchState(),
            LaunchState::kReadyToLaunch);

  startup_app_launcher_->LaunchApp();

  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kExtraSecondaryAppId));
}

class FakeChromeKioskLaunchController : public ChromeKioskLaunchController {
 public:
  void SetInstallResult(ChromeKioskInstallResult result) {
    install_result_ = result;
  }
  void SetLaunchResult(ChromeKioskLaunchResult result) {
    launch_result_ = result;
  }

  mojo::PendingRemote<ChromeKioskLaunchController> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // `ChromeKioskLaunchController`
  void InstallKioskApp(AppInstallParamsPtr params,
                       InstallKioskAppCallback callback) override {
    std::move(callback).Run(install_result_);
  }

  void LaunchKioskApp(const std::string& app_id,
                      bool is_network_ready,
                      LaunchKioskAppCallback callback) override {
    std::move(callback).Run(launch_result_);
  }

 private:
  mojo::Receiver<ChromeKioskLaunchController> receiver_{this};
  ChromeKioskInstallResult install_result_ = ChromeKioskInstallResult::kUnknown;
  ChromeKioskLaunchResult launch_result_ = ChromeKioskLaunchResult::kUnknown;
};

}  // namespace ash
