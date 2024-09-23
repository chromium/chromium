// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

#include <unistd.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/code_signature_mac.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

class MockDelegate : public AppShimManager::Delegate {
 public:
  ~MockDelegate() override {}

  MOCK_METHOD2(ShowAppWindows, bool(Profile*, const std::string&));
  MOCK_METHOD2(CloseAppWindows, void(Profile*, const std::string&));

  MOCK_METHOD2(AppIsInstalled, bool(Profile*, const std::string&));
  MOCK_METHOD2(AppUsesRemoteCocoa, bool(Profile*, const std::string&));
  MOCK_METHOD2(AppIsMultiProfile, bool(Profile*, const std::string&));
  MOCK_METHOD3(EnableExtension,
               void(Profile*, const std::string&, base::OnceCallback<void()>));
  MOCK_METHOD7(LaunchApp,
               void(Profile*,
                    const std::string& app_id,
                    const std::vector<base::FilePath>&,
                    const std::vector<GURL>&,
                    const GURL&,
                    chrome::mojom::AppShimLoginItemRestoreState,
                    base::OnceClosure));
  MOCK_METHOD2(GetAppShortcutsMenuItemInfos,
               std::vector<chrome::mojom::ApplicationDockMenuItemPtr>(
                   Profile*,
                   const std::string&));

  // Conditionally mock LaunchShim. Some tests will execute |launch_callback|
  // with a particular value.
  MOCK_METHOD4(DoLaunchShim,
               void(Profile*,
                    const std::string&,
                    web_app::LaunchShimUpdateBehavior,
                    web_app::ShimLaunchMode));
  void LaunchShim(Profile* profile,
                  const std::string& app_id,
                  web_app::LaunchShimUpdateBehavior update_behavior,
                  web_app::ShimLaunchMode launch_mode,
                  ShimLaunchedCallback launched_callback,
                  ShimTerminatedCallback terminated_callback) override {
    if (launch_shim_callback_capture_) {
      *launch_shim_callback_capture_ = std::move(launched_callback);
    }
    if (terminated_shim_callback_capture_) {
      *terminated_shim_callback_capture_ = std::move(terminated_callback);
    }
    DoLaunchShim(profile, app_id, update_behavior, launch_mode);
  }
  void SetCaptureShimLaunchedCallback(ShimLaunchedCallback* callback) {
    launch_shim_callback_capture_ = callback;
  }
  void SetCaptureShimTerminatedCallback(ShimTerminatedCallback* callback) {
    terminated_shim_callback_capture_ = callback;
  }

  MOCK_METHOD0(HasNonBookmarkAppWindowsOpen, bool());

  void SetAppCanCreateHost(bool should_create_host) {
    allow_shim_to_connect_ = should_create_host;
  }
  bool AppCanCreateHost(Profile* profile, const std::string& app_id) override {
    return allow_shim_to_connect_;
  }

 private:
  raw_ptr<ShimLaunchedCallback> launch_shim_callback_capture_ = nullptr;
  raw_ptr<ShimTerminatedCallback> terminated_shim_callback_capture_ = nullptr;
  bool allow_shim_to_connect_ = true;
};

class TestingAppShimManager : public AppShimManager {
 public:
  explicit TestingAppShimManager(std::unique_ptr<Delegate> delegate)
      : AppShimManager(std::move(delegate)) {}
  TestingAppShimManager(const TestingAppShimManager&) = delete;
  TestingAppShimManager& operator=(const TestingAppShimManager&) = delete;
  ~TestingAppShimManager() override { DCHECK(load_profile_callbacks_.empty()); }

  MOCK_METHOD1(OnShimFocus, void(AppShimHost* host));

  void RealOnShimFocus(AppShimHost* host) { AppShimManager::OnShimFocus(host); }

  void SetProfileMenuItems(
      std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items) {
    new_profile_menu_items_ = std::move(new_profile_menu_items);
    OnAvatarMenuChanged(nullptr);
  }
  void RebuildProfileMenuItemsFromAvatarMenu() override {
    profile_menu_items_.clear();
    for (const auto& item : new_profile_menu_items_) {
      profile_menu_items_.push_back(item.Clone());
    }
  }

  void SetAcceptablyCodeSigned(bool is_acceptable_code_signed) {
    is_acceptably_code_signed_ = is_acceptable_code_signed;
  }
  bool IsAcceptablyCodeSigned(audit_token_t audit_token) const override {
    return is_acceptably_code_signed_;
  }

  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath&));
  MOCK_METHOD1(ProfileForBackgroundShimLaunch,
               Profile*(const webapps::AppId& app_id));
  void LoadProfileAsync(const base::FilePath& path,
                        base::OnceCallback<void(Profile*)> callback) override {
    CaptureLoadProfileCallback(path, std::move(callback));
  }
  void WaitForAppRegistryReadyAsync(
      Profile* profile,
      base::OnceCallback<void()> callback) override {
    std::move(callback).Run();
  }
  MOCK_METHOD1(IsProfileLockedForPath, bool(const base::FilePath&));
  void SetHostForCreate(std::unique_ptr<AppShimHost> host_for_create) {
    host_for_create_ = std::move(host_for_create);
  }
  std::unique_ptr<AppShimHost> CreateHost(AppShimHost::Client* client,
                                          const base::FilePath& profile_path,
                                          const std::string& app_id,
                                          bool use_remote_cocoa) override {
    DCHECK(host_for_create_);
    std::unique_ptr<AppShimHost> result = std::move(host_for_create_);
    return result;
  }
  MOCK_METHOD2(OpenAppURLInBrowserWindow,
               void(const base::FilePath&, const GURL& url));
  MOCK_METHOD0(LaunchProfilePicker, void());
  MOCK_METHOD0(MaybeTerminate, void());

  void CaptureLoadProfileCallback(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback) {
    load_profile_callbacks_[path] = std::move(callback);
  }
  bool RunLoadProfileCallback(const base::FilePath& path, Profile* profile) {
    std::move(load_profile_callbacks_[path]).Run(profile);
    return load_profile_callbacks_.erase(path);
  }

 private:
  std::map<base::FilePath, base::OnceCallback<void(Profile*)>>
      load_profile_callbacks_;
  std::unique_ptr<AppShimHost> host_for_create_;
  std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items_;
  bool is_acceptably_code_signed_ = true;
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestingAppShimHostBootstrap(
      const base::FilePath& profile_path,
      const std::string& app_id,
      bool is_from_bookmark,
      std::optional<chrome::mojom::AppShimLaunchResult>* launch_result)
      : AppShimHostBootstrap(AuditTokenForCurrentProcess()),
        profile_path_(profile_path),
        app_id_(app_id),
        is_from_bookmark_(is_from_bookmark),
        launch_result_(launch_result),
        weak_factory_(this) {}
  TestingAppShimHostBootstrap(const TestingAppShimHostBootstrap&) = delete;
  TestingAppShimHostBootstrap& operator=(const TestingAppShimHostBootstrap&) =
      delete;

  void DoTestLaunch(
      chrome::mojom::AppShimLaunchType launch_type,
      const std::vector<base::FilePath>& files,
      const std::vector<GURL>& urls,
      chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
      mojo::PendingReceiver<
          mac_notifications::mojom::MacNotificationActionHandler>
          notification_action_handler) {
    mojo::Remote<chrome::mojom::AppShimHost> host;
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = profile_path_;
    app_shim_info->app_id = app_id_;
    if (is_from_bookmark_) {
      app_shim_info->app_url = GURL("https://example.com");
    }
    app_shim_info->launch_type = launch_type;
    app_shim_info->files = files;
    app_shim_info->urls = urls;
    app_shim_info->login_item_restore_state = login_item_restore_state;
    app_shim_info->notification_action_handler =
        std::move(notification_action_handler);
    OnShimConnected(
        host.BindNewPipeAndPassReceiver(), std::move(app_shim_info),
        base::BindOnce(&TestingAppShimHostBootstrap::DoTestLaunchDone,
                       launch_result_));
  }

  static void DoTestLaunchDone(
      std::optional<chrome::mojom::AppShimLaunchResult>* launch_result,
      chrome::mojom::AppShimLaunchResult result,
      variations::VariationsCommandLine feature_state,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    if (launch_result) {
      launch_result->emplace(result);
    }
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const base::FilePath profile_path_;
  const std::string app_id_;
  const bool is_from_bookmark_;
  // Note that |launch_result_| is optional so that we can track whether or not
  // the callback to set it has arrived.
  raw_ptr<std::optional<chrome::mojom::AppShimLaunchResult>> launch_result_ =
      nullptr;
  base::WeakPtrFactory<TestingAppShimHostBootstrap> weak_factory_;

  static audit_token_t AuditTokenForCurrentProcess() {
    audit_token_t token;
    mach_msg_type_number_t size = TASK_AUDIT_TOKEN_COUNT;
    int kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)&token,
                       &size);
    CHECK(kr == KERN_SUCCESS) << " Error getting audit token.";
    return token;
  }
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class TestAppShim : public chrome::mojom::AppShim,
                    public mac_notifications::mojom::MacNotificationProvider {
 public:
  // chrome::mojom::AppShim:
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void SetBadgeLabel(const std::string& badge_label) override {
    badge_label_ = badge_label;
  }
  void SetUserAttention(
      chrome::mojom::AppShimAttentionType attention_type) override {}
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override {
    profile_menu_items_ = std::move(profile_menu_items);
  }
  void UpdateApplicationDockMenu(
      std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items)
      override {
    dock_menu_items_ = std::move(dock_menu_items);
  }
  void BindNotificationProvider(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
          provider) override {
    notification_provider_receiver_.Bind(std::move(provider));
  }
  void RequestNotificationPermission(
      RequestNotificationPermissionCallback callback) override {
    request_notification_permission_callback_.SetValue(std::move(callback));
  }
  void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
          receiver) override {}

  // mac_notifications::mojom::MacNotificationProvider:
  void BindNotificationService(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler> handler)
      override {}

  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;
  std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items_;
  std::string badge_label_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationProvider>
      notification_provider_receiver_{this};
  base::test::TestFuture<RequestNotificationPermissionCallback>
      request_notification_permission_callback_;
};

class TestHost : public AppShimHost {
 public:
  TestHost(const base::FilePath& profile_path,
           const std::string& app_id,
           TestingAppShimManager* manager)
      : AppShimHost(manager,
                    app_id,
                    profile_path,
                    false /* uses_remote_views */),
        test_app_shim_(new TestAppShim),
        test_weak_factory_(this) {}
  TestHost(const TestHost&) = delete;
  TestHost& operator=(const TestHost&) = delete;
  ~TestHost() override {}

  chrome::mojom::AppShim* GetAppShim() const override {
    return test_app_shim_.get();
  }

  // Record whether or not OnBootstrapConnected has been called.
  void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override {
    EXPECT_FALSE(did_connect_to_host_);
    did_connect_to_host_ = true;
    AppShimHost::OnBootstrapConnected(std::move(bootstrap));
  }
  bool did_connect_to_host() const { return did_connect_to_host_; }

  base::WeakPtr<TestHost> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

  using AppShimHost::FilesOpened;
  using AppShimHost::NotificationPermissionStatusChanged;
  using AppShimHost::OpenAppWithOverrideUrl;
  using AppShimHost::ProfileSelectedFromMenu;
  using AppShimHost::ReopenApp;
  using AppShimHost::UrlsOpened;

  std::unique_ptr<TestAppShim> test_app_shim_;

 private:
  bool did_connect_to_host_ = false;

  base::WeakPtrFactory<TestHost> test_weak_factory_;
};

class AppShimManagerTest : public testing::Test {
 protected:
  AppShimManagerTest() {}
  AppShimManagerTest(const AppShimManagerTest&) = delete;
  AppShimManagerTest& operator=(const AppShimManagerTest&) = delete;
  ~AppShimManagerTest() override {}

  void SetUp() override {
    profile_path_a_ = profile_a_.GetPath();
    profile_path_b_ = profile_b_.GetPath();
    profile_path_c_ = profile_c_.GetPath();
    const base::FilePath user_data_dir = profile_path_a_.DirName();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    AppShimRegistry::Get()->RegisterLocalPrefs(local_state_->registry());
    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        local_state_.get(), user_data_dir);

    std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    manager_ = std::make_unique<TestingAppShimManager>(std::move(delegate));
    AppShimHostBootstrap::SetClient(manager_.get());
    bootstrap_aa_ = (new TestingAppShimHostBootstrap(
                         profile_path_a_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_aa_result_))
                        ->GetWeakPtr();
    bootstrap_ba_ = (new TestingAppShimHostBootstrap(
                         profile_path_b_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_ba_result_))
                        ->GetWeakPtr();
    bootstrap_ca_ = (new TestingAppShimHostBootstrap(
                         profile_path_c_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_ca_result_))
                        ->GetWeakPtr();
    bootstrap_xa_ = (new TestingAppShimHostBootstrap(
                         base::FilePath(), kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_xa_result_))
                        ->GetWeakPtr();
    bootstrap_ab_ = (new TestingAppShimHostBootstrap(
                         profile_path_a_, kTestAppIdB,
                         false /* is_from_bookmark */, &bootstrap_ab_result_))
                        ->GetWeakPtr();
    bootstrap_bb_ = (new TestingAppShimHostBootstrap(
                         profile_path_b_, kTestAppIdB,
                         false /* is_from_bookmark */, &bootstrap_bb_result_))
                        ->GetWeakPtr();
    bootstrap_aa_duplicate_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         true /* is_from_bookmark */,
                                         &bootstrap_aa_duplicate_result_))
            ->GetWeakPtr();
    bootstrap_aa_thethird_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         true /* is_from_bookmark */,
                                         &bootstrap_aa_thethird_result_))
            ->GetWeakPtr();

    host_aa_unique_ = std::make_unique<TestHost>(profile_path_a_, kTestAppIdA,
                                                 manager_.get());
    host_ab_unique_ = std::make_unique<TestHost>(profile_path_a_, kTestAppIdB,
                                                 manager_.get());
    host_ba_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdA,
                                                 manager_.get());
    host_bb_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdB,
                                                 manager_.get());
    host_aa_duplicate_unique_ = std::make_unique<TestHost>(
        profile_path_a_, kTestAppIdA, manager_.get());

    host_aa_ = host_aa_unique_->GetWeakPtr();
    host_ab_ = host_ab_unique_->GetWeakPtr();
    host_ba_ = host_ba_unique_->GetWeakPtr();
    host_bb_ = host_bb_unique_->GetWeakPtr();

    base::FilePath extension_path("/fake/path");

    EXPECT_CALL(*delegate_, AppIsMultiProfile(_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsMultiProfile(_, kTestAppIdB))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*delegate_, AppUsesRemoteCocoa(_, _))
        .WillRepeatedly(Return(true));

    {
      auto item_a = chrome::mojom::ProfileMenuItem::New();
      item_a->profile_path = profile_path_a_;
      item_a->menu_index = 0;
      auto item_b = chrome::mojom::ProfileMenuItem::New();
      item_b->profile_path = profile_path_b_;
      item_b->menu_index = 1;
      std::vector<chrome::mojom::ProfileMenuItemPtr> items;
      items.push_back(std::move(item_a));
      items.push_back(std::move(item_b));
      manager_->SetProfileMenuItems(std::move(items));
    }

    // Tests that expect this call will override it.
    EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(_, _)).Times(0);

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_a_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_b_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_c_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_c_))
        .WillRepeatedly(Return(&profile_c_));

    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_b_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_c_, kTestAppIdA))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, AppIsInstalled(_, kTestAppIdB))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, LaunchApp(_, _, _, _, _, _, _))
        .WillRepeatedly(Return());
  }

  void TearDown() override {
    host_aa_unique_.reset();
    host_ab_unique_.reset();
    host_ba_unique_.reset();
    host_bb_unique_.reset();
    host_aa_duplicate_unique_.reset();
    delegate_ = nullptr;
    manager_->SetHostForCreate(nullptr);
    manager_.reset();

    // Delete the bootstraps via their weak pointers if they haven't been
    // deleted yet. Note that this must be done after the profiles and hosts
    // have been destroyed (because they may now own the bootstraps).
    delete bootstrap_aa_.get();
    delete bootstrap_ba_.get();
    delete bootstrap_ca_.get();
    delete bootstrap_xa_.get();
    delete bootstrap_ab_.get();
    delete bootstrap_bb_.get();
    delete bootstrap_aa_duplicate_.get();
    delete bootstrap_aa_thethird_.get();

    AppShimHostBootstrap::SetClient(nullptr);

    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        nullptr, base::FilePath());
  }

  void DoShimLaunch(
      base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
      std::unique_ptr<TestHost> host,
      chrome::mojom::AppShimLaunchType launch_type,
      const std::vector<base::FilePath>& files,
      const std::vector<GURL>& urls,
      chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
      mojo::PendingReceiver<
          mac_notifications::mojom::MacNotificationActionHandler>
          notification_action_handler = mojo::NullReceiver()) {
    if (host) {
      manager_->SetHostForCreate(std::move(host));
    }
    bootstrap->DoTestLaunch(launch_type, files, urls, login_item_restore_state,
                            std::move(notification_action_handler));
  }

  void NormalLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host),
                 chrome::mojom::AppShimLaunchType::kNormal,
                 std::vector<base::FilePath>(), std::vector<GURL>(),
                 chrome::mojom::AppShimLoginItemRestoreState::kNone);
  }

  void RegisterOnlyLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                          std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host),
                 chrome::mojom::AppShimLaunchType::kRegisterOnly,
                 std::vector<base::FilePath>(), std::vector<GURL>(),
                 chrome::mojom::AppShimLoginItemRestoreState::kNone);
  }

  void NotificationActionLaunch(
      base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
      std::unique_ptr<TestHost> host,
      mac_notifications::mojom::NotificationActionInfoPtr notification) {
    mojo::Remote<mac_notifications::mojom::MacNotificationActionHandler>
        notification_remote;
    DoShimLaunch(bootstrap, std::move(host),
                 chrome::mojom::AppShimLaunchType::kNotificationAction,
                 std::vector<base::FilePath>(), std::vector<GURL>(),
                 chrome::mojom::AppShimLoginItemRestoreState::kNone,
                 notification_remote.BindNewPipeAndPassReceiver());
    notification_remote->OnNotificationAction(std::move(notification));
  }

  // Completely launch a shim host and leave it running.
  void LaunchAndActivate(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                         std::unique_ptr<TestHost> host_unique,
                         Profile* profile) {
    base::WeakPtr<TestHost> host = host_unique->GetWeakPtr();
    NormalLaunch(bootstrap, std::move(host_unique));
    EXPECT_EQ(host.get(), manager_->FindHost(profile, host->GetAppId()));
    EXPECT_CALL(*manager_, OnShimFocus(host.get()));
    manager_->OnAppActivated(profile, host->GetAppId());
    EXPECT_TRUE(host->did_connect_to_host());
  }

  // Simulates a focus request coming from a running app shim.
  void ShimNormalFocus(TestHost* host) {
    EXPECT_CALL(*manager_, OnShimFocus(host))
        .WillOnce(
            Invoke(manager_.get(), &TestingAppShimManager::RealOnShimFocus));
    manager_->OnShimFocus(host);
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<MockDelegate> delegate_ = nullptr;
  std::unique_ptr<TestingAppShimManager> manager_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  base::FilePath profile_path_c_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;
  TestingProfile profile_c_;

  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ba_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ca_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_xa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ab_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_bb_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_duplicate_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_thethird_;

  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_aa_result_;
  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_ba_result_;
  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_ca_result_;
  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_xa_result_;
  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_ab_result_;
  std::optional<chrome::mojom::AppShimLaunchResult> bootstrap_bb_result_;
  std::optional<chrome::mojom::AppShimLaunchResult>
      bootstrap_aa_duplicate_result_;
  std::optional<chrome::mojom::AppShimLaunchResult>
      bootstrap_aa_thethird_result_;

  // Unique ptr to the TestsHosts used by the tests. These are passed by
  // std::move during tests. To access them after they have been passed, use
  // the WeakPtr versions.
  std::unique_ptr<TestHost> host_aa_unique_;
  std::unique_ptr<TestHost> host_ab_unique_;
  std::unique_ptr<TestHost> host_ba_unique_;
  std::unique_ptr<TestHost> host_bb_unique_;
  std::unique_ptr<TestHost> host_aa_duplicate_unique_;

  base::WeakPtr<TestHost> host_aa_;
  base::WeakPtr<TestHost> host_ab_;
  base::WeakPtr<TestHost> host_ba_;
  base::WeakPtr<TestHost> host_bb_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

TEST_F(AppShimManagerTest, LaunchProfileNotFound) {
  // Bad profile path, opens a bookmark app in a new window.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(static_cast<Profile*>(nullptr)));
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(profile_path_a_, _));
  manager_->RunLoadProfileCallback(profile_path_a_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileNotFound,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchProfileNotFoundNotBookmark) {
  // Bad profile path, not a bookmark app, doesn't open anything.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(static_cast<Profile*>(nullptr)));
  NormalLaunch(bootstrap_ab_, nullptr);
  manager_->RunLoadProfileCallback(profile_path_a_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileNotFound,
            *bootstrap_ab_result_);
}

TEST_F(AppShimManagerTest, LaunchProfileIsLocked) {
  // Profile is locked.
  EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_a_))
      .WillOnce(Return(true));
  EXPECT_CALL(*manager_, LaunchProfilePicker());
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileLocked,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchAppNotFound) {
  // App not found.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(profile_path_a_, _));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kAppNotFound,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchAppNotEnabled) {
  // App not found.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
}

TEST_F(AppShimManagerTest, LaunchAndCloseShim) {
  // Normal startup.
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  std::vector<GURL> some_url(1, GURL("web+test://foo"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdB, some_file, some_url, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  DoShimLaunch(bootstrap_bb_, std::move(host_bb_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file, some_url,
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));

  // Activation when there is a registered shim finishes launch with success and
  // focuses the app.
  EXPECT_CALL(*manager_, OnShimFocus(host_aa_.get()));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);

  // Starting and closing a second host does nothing.
  DoShimLaunch(bootstrap_aa_duplicate_, std::move(host_aa_duplicate_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file, some_url,
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kDuplicateHost,
            *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_EQ(host_aa_.get(), nullptr);
}

TEST_F(AppShimManagerTest, RunOnOsLoginLaunchAndCloseShim) {
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  // Run on OS Login Launch
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  std::vector<GURL> some_url(1, GURL("web+test://foo"));
  EXPECT_CALL(
      *delegate_,
      LaunchApp(&profile_b_, kTestAppIdB, some_file, some_url, _,
                chrome::mojom::AppShimLoginItemRestoreState::kWindowed, _));
  DoShimLaunch(bootstrap_bb_, std::move(host_bb_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file, some_url,
               chrome::mojom::AppShimLoginItemRestoreState::kWindowed);
  EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));

  // Activation when there is a registered shim finishes launch with success and
  // focuses the app.
  EXPECT_CALL(*manager_, OnShimFocus(host_aa_.get()));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);

  // Starting and closing a second host does nothing.
  DoShimLaunch(bootstrap_aa_duplicate_, std::move(host_aa_duplicate_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file, some_url,
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kDuplicateHost,
            *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_EQ(host_aa_.get(), nullptr);
}

TEST_F(AppShimManagerTest, AppLaunchCancelled) {
  // Validate that if no browser is registered during a launch that
  // OnAppLaunchCancelled removes the host and closes the app.
  NormalLaunch(bootstrap_bb_, std::move(host_bb_unique_));
  EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));
  EXPECT_CALL(*manager_, MaybeTerminate()).WillOnce(Return());
  manager_->OnAppLaunchCancelled(&profile_b_, kTestAppIdB);
  EXPECT_FALSE(manager_->FindHost(&profile_b_, kTestAppIdB));
  EXPECT_EQ(host_bb_.get(), nullptr);

  // Validate that if a browser is registered during a launch
  // that OnAppLaunchCancelled is an no-op
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Notify manager that a new browser has been associated with the app.
  auto browser_window = std::make_unique<TestBrowserWindow>();
  std::string app_name = web_app::GenerateApplicationNameFromAppId(kTestAppIdA);
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      app_name, true, browser_window->GetBounds(), &profile_a_, true);
  params.window = browser_window.get();
  auto browser = std::unique_ptr<Browser>(Browser::Create(params));
  manager_->OnBrowserAdded(browser.get());

  // Validate that OnAppLaunchCancelled does not close the app,
  // and that the state is still valid.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppLaunchCancelled(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Removing the browser should close the app.
  EXPECT_CALL(*manager_, MaybeTerminate()).WillOnce(Return());
  manager_->OnBrowserRemoved(browser.get());
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, AppLifetime) {
  scoped_feature_list_.InitWithFeatures({features::kAppShimNewCloseBehavior},
                                        {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // When the app activates, a host is created. If there is no shim, one is
  // launched.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal shim launch adds an entry in the map.
  // App should not be launched here, but return success to the shim.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Return no app windows for OnShimFocus. This will do nothing.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(0);
  ShimNormalFocus(host_aa_.get());

  // Return no app windows for OnShimReopen. This will result in a launch call.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(1);
  host_aa_->ReopenApp();

  // Return one window. This should do nothing.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  host_aa_->ReopenApp();

  // Open files should trigger a launch with those files.
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(
      *delegate_,
      LaunchApp(&profile_a_, kTestAppIdA, some_file, std::vector<GURL>(),
                GURL(), chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  host_aa_->FilesOpened(some_file);

  // Open urls should trigger a launch with those urls
  std::vector<GURL> some_url(1, GURL("web+test://foo"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, std::vector<base::FilePath>(),
                        some_url, GURL(),
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  host_aa_->UrlsOpened(some_url);

  // Open app with override url should trigger a launch with that url
  GURL some_override_url("https://some-override-url.com");
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, std::vector<base::FilePath>(),
                        std::vector<GURL>(), some_override_url,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  host_aa_->OpenAppWithOverrideUrl(some_override_url);

  // OnAppDeactivated should not close the shim.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);
  EXPECT_NE(nullptr, host_aa_.get());

  // Process disconnect will cause the shim to close.
  EXPECT_CALL(*manager_, MaybeTerminate()).WillOnce(Return());
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, AppLifetimeOld) {
  scoped_feature_list_.InitWithFeatures({},
                                        {features::kAppShimNewCloseBehavior});

  // When the app activates, a host is created. If there is no shim, one is
  // launched.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal shim launch adds an entry in the map.
  // App should not be launched here, but return success to the shim.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Return no app windows for OnShimFocus. This will do nothing.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(0);
  ShimNormalFocus(host_aa_.get());

  // Return no app windows for OnShimReopen. This will result in a launch call.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(1);
  host_aa_->ReopenApp();

  // Return one window. This should do nothing.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _, _, _, _, _))
      .Times(0);
  host_aa_->ReopenApp();

  // Open files should trigger a launch with those files.
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, some_file,
                                    std::vector<GURL>(), GURL(), _, _));
  host_aa_->FilesOpened(some_file);

  // Open urls should trigger a launch with those urls
  std::vector<GURL> some_url(1, GURL("web+test://foo"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, std::vector<base::FilePath>(),
                        some_url, GURL(), _, _));
  host_aa_->UrlsOpened(some_url);

  // Open app with override url should trigger a launch with that url
  GURL some_override_url("https://some-override-url.com");
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, std::vector<base::FilePath>(),
                        std::vector<GURL>(), some_override_url, _, _));
  host_aa_->OpenAppWithOverrideUrl(some_override_url);

  // Process disconnect will cause the host to be deleted.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());

  // OnAppDeactivated should trigger a MaybeTerminate call.
  EXPECT_CALL(*manager_, MaybeTerminate()).WillOnce(Return());
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);
}

TEST_F(AppShimManagerTest, FailToLaunch) {
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // When the app activates, it requests a launch.
  ShimLaunchedCallback launch_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launch_callback);
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launch_callback);

  // Run the callback claiming that the launch failed. This should trigger
  // another launch, this time forcing shim recreation.
  EXPECT_CALL(
      *delegate_,
      DoLaunchShim(&profile_a_, kTestAppIdA,
                   web_app::LaunchShimUpdateBehavior::kRecreateUnconditionally,
                   web_app::ShimLaunchMode::kNormal));
  std::move(launch_callback).Run(base::Process());
  EXPECT_TRUE(launch_callback);

  // Report that the launch failed. This should trigger deletion of the host.
  EXPECT_NE(nullptr, host_aa_.get());
  std::move(launch_callback).Run(base::Process());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, FailToConnect) {
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // When the app activates, it requests a launch.
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Report that the process terminated. This should trigger a re-create and
  // re-launch.
  EXPECT_CALL(
      *delegate_,
      DoLaunchShim(&profile_a_, kTestAppIdA,
                   web_app::LaunchShimUpdateBehavior::kRecreateUnconditionally,
                   web_app::ShimLaunchMode::kNormal));
  std::move(terminated_callback).Run();
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(7));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Report that the process terminated again. This should trigger deletion of
  // the host.
  EXPECT_NE(nullptr, host_aa_.get());
  std::move(terminated_callback).Run();
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, FailCodeSignature) {
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  manager_->SetAcceptablyCodeSigned(false);
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Fail to code-sign. This should result in a host being created, and a launch
  // having been requested.
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Simulate the register call that then fails due to signature failing.
  RegisterOnlyLaunch(bootstrap_aa_duplicate_, std::move(host_aa_unique_));
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Simulate the termination after the register failed.
  manager_->SetAcceptablyCodeSigned(true);
  EXPECT_CALL(
      *delegate_,
      DoLaunchShim(&profile_a_, kTestAppIdA,
                   web_app::LaunchShimUpdateBehavior::kRecreateUnconditionally,
                   web_app::ShimLaunchMode::kNormal));
  std::move(terminated_callback).Run();
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  RegisterOnlyLaunch(bootstrap_aa_thethird_, std::move(host_aa_unique_));
  EXPECT_TRUE(host_aa_->HasBootstrapConnected());
}

TEST_F(AppShimManagerTest, MaybeTerminate) {
  scoped_feature_list_.InitWithFeatures({features::kAppShimNewCloseBehavior},
                                        {});

  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdB,
                                                   profile_path_a_);

  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  // Quitting when there's another shim should not terminate.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim should not terminate in the new behavior.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(AppShimManagerTest, MaybeTerminateOnUninstall) {
  scoped_feature_list_.InitWithFeatures({features::kAppShimNewCloseBehavior},
                                        {});

  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdB,
                                                   profile_path_a_);

  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  // Quitting when there's another shim should not terminate.
  AppShimRegistry::Get()->OnAppUninstalledForProfile(kTestAppIdA,
                                                     profile_path_a_);
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim and the app is uninstalled should
  // terminate.
  AppShimRegistry::Get()->OnAppUninstalledForProfile(kTestAppIdB,
                                                     profile_path_a_);
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(1);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(AppShimManagerTest, MaybeTerminateOld) {
  scoped_feature_list_.InitWithFeatures({},
                                        {features::kAppShimNewCloseBehavior});

  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  // Quitting when there's another shim should not terminate.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim should terminate.
  EXPECT_CALL(*manager_, MaybeTerminate());
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(AppShimManagerTest, RegisterOnly) {
  // For an chrome::mojom::AppShimLaunchType::kRegisterOnly, don't launch the
  // app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _, _, _, _, _)).Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_TRUE(manager_->FindHost(&profile_a_, kTestAppIdA));

  // Close the shim, removing the entry in the map.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, DontCreateHost) {
  delegate_->SetAppCanCreateHost(false);

  // The app should be launched.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _, _, _, _, _)).Times(1);
  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  // But the bootstrap should be closed.
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect,
            *bootstrap_ab_result_);
  // And we should create no host.
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdB));
}

TEST_F(AppShimManagerTest, NotificationAction) {
  class AppShimObserver : public AppShimManager::AppShimObserver {
   public:
    void OnShimProcessConnectedAndAllLaunchesDone(
        base::ProcessId pid,
        chrome::mojom::AppShimLaunchResult result) override {
      launch_result_.SetValue(result);
    }
    bool OnNotificationAction(
        mac_notifications::mojom::NotificationActionInfoPtr& info) override {
      notification_action_.SetValue(std::move(info));
      return false;
    }

    base::test::TestFuture<chrome::mojom::AppShimLaunchResult> launch_result_;
    base::test::TestFuture<mac_notifications::mojom::NotificationActionInfoPtr>
        notification_action_;
  };

  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // Use SetAppCanCreateHost to simulate the case where there isn't already a
  // loaded profile.
  delegate_->SetAppCanCreateHost(false);

  AppShimObserver observer;
  manager_->SetAppShimObserverForTesting(&observer);

  // Create a test notification action.
  auto profile_identifier = mac_notifications::mojom::ProfileIdentifier::New(
      profile_a_.GetBaseName().AsUTF8Unsafe(), /*incognito=*/false);
  auto notification_identifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          "notificaiton-id", std::move(profile_identifier));
  auto notification = mac_notifications::mojom::NotificationActionInfo::New();
  notification->meta = mac_notifications::mojom::NotificationMetadata::New(
      std::move(notification_identifier), /*notification_type=*/0,
      /*origin_url=*/GURL("https://example.com"), /*user_data_dir=*/"");
  notification->operation = NotificationOperation::kClick;
  notification->button_index = -1;

  // For an chrome::mojom::AppShimLaunchType::kNotificationAction, don't launch
  // the app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _, _, _, _, _)).Times(0);
  NotificationActionLaunch(bootstrap_aa_, std::move(host_aa_unique_),
                           std::move(notification));
  // Should not have a result yet since the notification action hasn't been
  // handled yet.
  EXPECT_FALSE(bootstrap_aa_result_.has_value());
  EXPECT_FALSE(observer.launch_result_.IsReady());
  EXPECT_FALSE(observer.notification_action_.IsReady());

  // Wait for the notification action to be handled.
  ASSERT_TRUE(observer.notification_action_.Wait());
  EXPECT_FALSE(observer.launch_result_.IsReady());

  // Which should now allow to launch to finish.
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect,
            observer.launch_result_.Get());
  ASSERT_TRUE(bootstrap_aa_result_.has_value());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect,
            *bootstrap_aa_result_);
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, LoadProfile) {
  // If the profile is not loaded when an OnShimProcessConnected arrives, return
  // false and load the profile asynchronously. Launch the app when the profile
  // is ready.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillOnce(Return(static_cast<Profile*>(nullptr)))
      .WillRepeatedly(Return(&profile_a_));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->RunLoadProfileCallback(profile_path_a_, &profile_a_);
  EXPECT_TRUE(manager_->FindHost(&profile_a_, kTestAppIdA));
}

// Tests that calls to OnShimFocus, OnShimHide correctly handle a null extension
// being provided by the extension system.
TEST_F(AppShimManagerTest, ExtensionUninstalled) {
  LaunchAndActivate(bootstrap_aa_, std::move(host_aa_unique_), &profile_a_);

  ShimNormalFocus(host_aa_.get());
  EXPECT_NE(nullptr, host_aa_.get());

  // Set up the mock to return a null extension, as if it were uninstalled.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));

  // Trying to focus will do nothing -- the shim will have to be closed by
  // the user manually.
  ShimNormalFocus(host_aa_.get());
  EXPECT_NE(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, PreExistingHost) {
  // Create a host for our profile.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal))
      .Times(1);
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for this host. It should find the pre-existing host, and the
  // pre-existing host's launch result should be set.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  EXPECT_FALSE(host_aa_->did_connect_to_host());
  DoShimLaunch(bootstrap_aa_, nullptr,
               chrome::mojom::AppShimLaunchType::kRegisterOnly,
               std::vector<base::FilePath>(), std::vector<GURL>(),
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Try to launch the app again. It should fail to launch, and the previous
  // profile should remain.
  DoShimLaunch(bootstrap_aa_duplicate_, nullptr,
               chrome::mojom::AppShimLaunchType::kRegisterOnly,
               std::vector<base::FilePath>(), std::vector<GURL>(),
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kDuplicateHost,
            *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, MultiProfile) {
  // Test with a bookmark app (host is shared).
  {
    // Create a host for profile A.
    manager_->SetHostForCreate(std::move(host_aa_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
    manager_->OnAppActivated(&profile_a_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());

    // Ensure that profile B has the same host.
    manager_->SetHostForCreate(std::move(host_ba_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdA));
    manager_->OnAppActivated(&profile_b_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());
  }

  // Test with a non-bookmark app (host is not shared).
  {
    // Create a host for profile A.
    manager_->SetHostForCreate(std::move(host_ab_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdB));
    manager_->OnAppActivated(&profile_a_, kTestAppIdB);
    EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));
    EXPECT_FALSE(host_ab_->did_connect_to_host());

    // Ensure that profile B has the same host.
    manager_->SetHostForCreate(std::move(host_bb_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdB));
    manager_->OnAppActivated(&profile_b_, kTestAppIdB);
    EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));
    EXPECT_FALSE(host_bb_->did_connect_to_host());
  }
}

TEST_F(AppShimManagerTest, MultiProfileShimLaunch) {
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for profile B. This should not cause a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _, _)).Times(0);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);
}

TEST_F(AppShimManagerTest, MultiProfileSelectMenu) {
  EXPECT_CALL(*delegate_, ShowAppWindows(_, _)).WillRepeatedly(Return(false));
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Select profile B from the menu. This should request that the app be
  // launched.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _, _)).Times(0);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Select profile A and B from the menu -- this should not request a launch,
  // because the profiles are already enabled.
  EXPECT_CALL(*delegate_, ShowAppWindows(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_,
              LaunchApp(_, _, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  host_aa_->ProfileSelectedFromMenu(profile_path_a_);
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
}

namespace {
// A helper that records when Show is called on a BrowserWindow to verify
// activation of existing browser windows.
class TestBrowserWindowShow : public TestBrowserWindow {
 public:
  void Show() override { did_show = true; }

  bool did_show = false;
};
}  // namespace

TEST_F(AppShimManagerTest, MultiProfileSelectMenu_ShowsBrowser) {
  EXPECT_CALL(*delegate_, ShowAppWindows(_, _)).WillRepeatedly(Return(false));
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Notify manager that a new browser has been associated with the app.
  auto browser_window_a = std::make_unique<TestBrowserWindowShow>();
  std::string app_name = web_app::GenerateApplicationNameFromAppId(kTestAppIdA);
  Browser::CreateParams params_a = Browser::CreateParams::CreateForApp(
      app_name, true, browser_window_a->GetBounds(), &profile_a_, true);
  params_a.window = browser_window_a.get();
  auto browser_a = std::unique_ptr<Browser>(Browser::Create(params_a));
  manager_->OnBrowserAdded(browser_a.get());

  // Select profile B from the menu. This should request that the app be
  // launched.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _));
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _, _)).Times(0);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Notify manager that a new browser has been associated with the app.
  auto browser_window_b = std::make_unique<TestBrowserWindowShow>();
  Browser::CreateParams params_b = Browser::CreateParams::CreateForApp(
      app_name, true, browser_window_b->GetBounds(), &profile_b_, true);
  params_b.window = browser_window_b.get();
  auto browser_b = std::unique_ptr<Browser>(Browser::Create(params_b));
  manager_->OnBrowserAdded(browser_b.get());

  EXPECT_FALSE(browser_window_a->did_show);
  EXPECT_FALSE(browser_window_b->did_show);

  // Select profile A and B from the menu -- this should not request a launch,
  // because the profiles are already enabled.
  EXPECT_CALL(*delegate_, ShowAppWindows(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_,
              LaunchApp(_, _, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  host_aa_->ProfileSelectedFromMenu(profile_path_a_);
  EXPECT_TRUE(browser_window_a->did_show);
  EXPECT_FALSE(browser_window_b->did_show);
  browser_window_a->did_show = false;

  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
  EXPECT_FALSE(browser_window_a->did_show);
  EXPECT_TRUE(browser_window_b->did_show);
}

TEST_F(AppShimManagerTest, ProfileMenuOneProfile) {
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    manager_->SetProfileMenuItems(std::move(items));
  }

  // Set this app to be installed for profile A.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // When the app activates, a host is created. This will trigger building
  // the avatar menu.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Launch the shim.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  const auto& menu_items = host_aa_->test_app_shim_->profile_menu_items_;

  // We should have no menu items, because there is only one installed profile.
  EXPECT_TRUE(menu_items.empty());

  // Add profile B to the avatar menu and call the avatar menu observer update
  // method.
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    auto item_b = chrome::mojom::ProfileMenuItem::New();
    item_b->profile_path = profile_path_b_;
    item_b->menu_index = 111;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    items.push_back(std::move(item_b));
    manager_->SetProfileMenuItems(std::move(items));
  }

  // We should still only have no menu items, because the app is not installed
  // for multiple profiles.
  EXPECT_TRUE(menu_items.empty());

  // Now install for profile B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);
  EXPECT_EQ(menu_items.size(), 2u);
  EXPECT_EQ(menu_items[0]->profile_path, profile_path_b_);
  EXPECT_EQ(menu_items[1]->profile_path, profile_path_a_);
}

TEST_F(AppShimManagerTest, FindProfileFromBadProfile) {
  // Set this app to be installed for profile A and B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);

  // Set the app to be last-active on profile A.
  std::set<base::FilePath> last_active_profile_paths;
  last_active_profile_paths.insert(profile_path_a_);
  AppShimRegistry::Get()->SaveLastActiveProfilesForApp(
      kTestAppIdA, last_active_profile_paths);

  // Launch the shim requesting profile C.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(1);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  EXPECT_CALL(*delegate_, EnableExtension(&profile_c_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_ca_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ca_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, FindProfileFromNoProfile) {
  // Set this app to be installed for profile A.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // Launch the shim without specifying a profile.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(1);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  NormalLaunch(bootstrap_xa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_xa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, FindProfileFromFilePaths) {
  // Set this app to be install for profile A, B and C.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_c_);

  // Configure different file handlers in each of the three profiles.
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_a_, {".md"}, {});
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_b_, {".txt", ".csv"}, {});
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_c_, {".txt"}, {});

  // Launch the shim passing in several files.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(1);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_c_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  DoShimLaunch(bootstrap_xa_, std::move(host_ba_unique_),
               chrome::mojom::AppShimLaunchType::kNormal,
               {base::FilePath("/foo/bar/test.txt"),
                base::FilePath("/home/test/data.csv"),
                base::FilePath("/data/README.md")},
               {}, chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_xa_result_);
  EXPECT_EQ(host_ba_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, FindProfileFromFileURL) {
  // Set this app to be install for profile A, B and C.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_c_);

  // Configure different file handlers in each of the three profiles.
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_a_, {".md"}, {});
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_b_, {".txt", ".csv"}, {});
  AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
      kTestAppIdA, profile_path_c_, {".txt"}, {});

  // Launch the shim passing in a file URL.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(1);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_c_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  DoShimLaunch(bootstrap_xa_, std::move(host_aa_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, {},
               {GURL("file:///data/README.md")},
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_xa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, FindProfileFromURL) {
  // Set this app to be install for profile A, B and C.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_c_);

  // Configure different protocol handlers in each of the three profiles.
  AppShimRegistry::Get()->SaveProtocolHandlersForAppAndProfile(
      kTestAppIdA, profile_path_a_, {"web+music"});
  AppShimRegistry::Get()->SaveProtocolHandlersForAppAndProfile(
      kTestAppIdA, profile_path_b_, {"web+jngl"});
  AppShimRegistry::Get()->SaveProtocolHandlersForAppAndProfile(
      kTestAppIdA, profile_path_c_, {"mailto"});

  // Launch the shim passing in a URL to be handled in profile B.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(1);
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_c_, kTestAppIdA, _, _, _,
                        chrome::mojom::AppShimLoginItemRestoreState::kNone, _))
      .Times(0);
  DoShimLaunch(bootstrap_xa_, std::move(host_ba_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, {},
               {GURL("web+jngl://foo/bar")},
               chrome::mojom::AppShimLoginItemRestoreState::kNone);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_xa_result_);
  EXPECT_EQ(host_ba_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, UpdateAppBadge) {
  // Set this app to be installed for profile A and B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);

  // Activate the app for profile_a_ and profile_b_
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  manager_->SetHostForCreate(std::move(host_ba_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdA));
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));

  // And update the badge in either profile, verifying that the combined value
  // is reflected.
  EXPECT_EQ("", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_a_, kTestAppIdA,
                           badging::BadgeManager::BadgeValue(4));
  EXPECT_EQ("4", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_b_, kTestAppIdA,
                           badging::BadgeManager::BadgeValue(3));
  EXPECT_EQ("7", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_a_, kTestAppIdA,
                           badging::BadgeManager::BadgeValue());
  EXPECT_EQ("3", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_b_, kTestAppIdA,
                           badging::BadgeManager::BadgeValue());
  EXPECT_EQ("•", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_a_, kTestAppIdA, std::nullopt);
  EXPECT_EQ("•", host_aa_->test_app_shim_->badge_label_);
  manager_->UpdateAppBadge(&profile_b_, kTestAppIdA, std::nullopt);
  EXPECT_EQ("", host_aa_->test_app_shim_->badge_label_);
}

TEST_F(AppShimManagerTest, UpdateApplicationDockMenu) {
  // Set this app to be installed for profile A and B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);

  struct DockMenuItems {
    std::u16string name;
    GURL url;
  };

  DockMenuItems menu_items_profile_a[] = {
      {u"dock_menu_item_a_1", GURL(".")},
      {u"dock_menu_item_a_2", GURL("/settings")},
      {u"dock_menu_item_a_3", GURL("https://anothersite.com")},
  };
  const size_t kNumMenuItemsForProfileA = std::size(menu_items_profile_a);

  DockMenuItems menu_items_profile_b[] = {
      {u"dock_menu_item_b_1", GURL("/about")},
      {u"dock_menu_item_b_2", GURL("/another-link")},
  };
  const size_t kNumMenuItemsForProfileB = std::size(menu_items_profile_b);

  // Lambda to help with creation of application dock menu items.
  auto MakeDockMenuItems = [](DockMenuItems* menu_items,
                              size_t menu_items_size) {
    std::vector<chrome::mojom::ApplicationDockMenuItemPtr> mock_dock_menu_items;
    for (size_t i = 0; i < menu_items_size; i++) {
      auto dock_menu_item = chrome::mojom::ApplicationDockMenuItem::New();
      dock_menu_item->name = menu_items[i].name;
      dock_menu_item->url = menu_items[i].url;
      mock_dock_menu_items.push_back(std::move(dock_menu_item));
    }
    return mock_dock_menu_items;
  };

  auto ValidateDockMenuItems = [&](DockMenuItems* expected_menu_items,
                                   size_t expected_menu_items_size) {
    const auto& dock_menu_items = host_aa_->test_app_shim_->dock_menu_items_;
    EXPECT_EQ(expected_menu_items_size, dock_menu_items.size());
    for (size_t i = 0; i < dock_menu_items.size(); i++) {
      EXPECT_EQ(expected_menu_items[i].name, dock_menu_items[i]->name);
      EXPECT_EQ(expected_menu_items[i].url, dock_menu_items[i]->url);
    }
  };

  // Activate the app for profile_a_ and profile_b_
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  manager_->SetHostForCreate(std::move(host_ba_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdA));
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));

  // Validate no application dock menu items have been set yet.
  ValidateDockMenuItems(nullptr, 0);

  // Create browser objects that can be passed via OnBrowserSetLastActive.
  std::string app_name = web_app::GenerateApplicationNameFromAppId(kTestAppIdA);
  std::unique_ptr<Browser> browser_profile_a, browser_profile_b;

  {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        app_name, true, browser_window->GetBounds(), &profile_a_, true);
    params.window = browser_window.get();
    browser_profile_a = std::unique_ptr<Browser>(Browser::Create(params));
  }

  {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        app_name, true, browser_window->GetBounds(), &profile_b_, true);
    params.window = browser_window.get();
    browser_profile_b = std::unique_ptr<Browser>(Browser::Create(params));
  }

  // Set profile A browser as last active, and validate the application dock
  // menu items.
  EXPECT_CALL(*delegate_,
              GetAppShortcutsMenuItemInfos(&profile_a_, kTestAppIdA))
      .WillOnce(Return(testing::ByMove(
          MakeDockMenuItems(menu_items_profile_a, kNumMenuItemsForProfileA))));

  manager_->OnBrowserSetLastActive(browser_profile_a.get());
  ValidateDockMenuItems(menu_items_profile_a, kNumMenuItemsForProfileA);

  // Set profile B browser as last active, and validate the application dock
  // menu items.
  EXPECT_CALL(*delegate_,
              GetAppShortcutsMenuItemInfos(&profile_b_, kTestAppIdA))
      .WillOnce(Return(testing::ByMove(
          MakeDockMenuItems(menu_items_profile_b, kNumMenuItemsForProfileB))));

  manager_->OnBrowserSetLastActive(browser_profile_b.get());
  ValidateDockMenuItems(menu_items_profile_b, kNumMenuItemsForProfileB);
}

TEST_F(AppShimManagerTest,
       BuildAppShimRequirementStringFromFrameworkRequirementStringTest) {
  EXPECT_TRUE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier \"com.google.Chrome.framework\" and certificate "
                "leaf = H\"c9a99324ca3fcb23dbcc36bd5fd4f9753305130a\"")));
  EXPECT_TRUE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier \"com.google.Chrome.framework\" and certificate "
                "leaf[subject.OU] = \"42HXZ8M8AV\"")));

  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR(
              "cdhash H\"daa66a31aeb85125bd2459bebf548b2dff5ee83b\" or cdhash "
              "H\"a8e5300bf9223510fc5b107b23de0d12f419acac\"")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier \"com.google.Chrome.framework\"")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("malformed")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("\"\"\"")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("\"\"")));
  EXPECT_FALSE(
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("\"")));

  // Crafted to pass all our requirement checks but fail
  // SecRequirementCreateWithString().
  base::apple::ScopedCFTypeRef<CFStringRef> requirement_string =
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier \"com.google.Chrome.framework\" and fail here"));
  EXPECT_TRUE(requirement_string);
  EXPECT_FALSE(apps::RequirementFromString(requirement_string.get()));
  // Missing quote in the post "identifier" portion which is caught by
  // SecRequirementCreateWithString().
  requirement_string =
      manager_->BuildAppShimRequirementStringFromFrameworkRequirementString(
          CFSTR("identifier \"com.google.Chrome.framework\" and certificate "
                "leaf = Hc9a99324ca3fcb23dbcc36bd5fd4f9753305130a\""));
  EXPECT_TRUE(requirement_string);
  EXPECT_FALSE(apps::RequirementFromString(requirement_string.get()));

  CFStringRef framework_req_string = CFSTR(
      "identifier \"com.google.Chrome.framework\" and anchor "
      "apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] /* "
      "exists */ and certificate leaf[field.1.2.840.113635.100.6.1.13] "
      "/* exists */ and certificate leaf[subject.OU] = EQHXZ8M8AV");
  base::apple::ScopedCFTypeRef<SecRequirementRef> got_req(
      apps::RequirementFromString(
          manager_
              ->BuildAppShimRequirementStringFromFrameworkRequirementString(
                  framework_req_string)
              .get()));
  ASSERT_TRUE(got_req);
  base::apple::ScopedCFTypeRef<CFStringRef> got_req_string;
  ASSERT_EQ(SecRequirementCopyString(got_req.get(), kSecCSDefaultFlags,
                                     got_req_string.InitializeInto()),
            errSecSuccess);
  CFStringRef want_req_string = CFSTR(
      "identifier \"app_mode_loader\" and anchor "
      "apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] /* "
      "exists */ and certificate leaf[field.1.2.840.113635.100.6.1.13] "
      "/* exists */ and certificate leaf[subject.OU] = EQHXZ8M8AV");
  EXPECT_EQ(base::SysCFStringRefToUTF8(got_req_string.get()),
            base::SysCFStringRefToUTF8(want_req_string));
}

TEST_F(AppShimManagerTest, LaunchNotificationProviderWithAppRunning) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // Launch the app shim.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Connect to its notification provider.
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> provider =
      manager_->LaunchNotificationProvider(kTestAppIdA);
  EXPECT_TRUE(provider.is_bound());
  EXPECT_TRUE(
      host_aa_->test_app_shim_->notification_provider_receiver_.is_bound());
  provider.FlushForTesting();
  EXPECT_TRUE(provider.is_connected());
}

TEST_F(AppShimManagerTest, LaunchNotificationProviderWithoutAppRunning) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(&profile_a_));

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kBackground));

  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> provider =
      manager_->LaunchNotificationProvider(kTestAppIdA);
  EXPECT_TRUE(provider.is_bound());
  EXPECT_TRUE(
      host_aa_->test_app_shim_->notification_provider_receiver_.is_bound());
  provider.FlushForTesting();
  EXPECT_TRUE(provider.is_connected());
}

TEST_F(AppShimManagerTest, LaunchNotificationProviderWithAppNotInstalled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(nullptr));

  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> provider =
      manager_->LaunchNotificationProvider(kTestAppIdA);
  // AppShimManager binds `provider` to a dummy implementation if an app can't
  // be found, since notifications code expects to always get a bound provider.
  EXPECT_TRUE(provider.is_bound());
  provider.FlushForTesting();
  EXPECT_TRUE(provider.is_connected());

  // Attempting to bind a notification service on the dummy provider should
  // immediately disconnect.
  mojo::Remote<mac_notifications::mojom::MacNotificationService> service;
  mojo::PendingReceiver<mac_notifications::mojom::MacNotificationActionHandler>
      handler;
  provider->BindNotificationService(service.BindNewPipeAndPassReceiver(),
                                    handler.InitWithNewPipeAndPassRemote());
  EXPECT_TRUE(service.is_connected());
  service.FlushForTesting();
  EXPECT_FALSE(service.is_connected());
  EXPECT_TRUE(provider.is_connected());
}

TEST_F(AppShimManagerTest, RequestNotificationPermissionWithAppRunning) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // Launch the app shim.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());
  EXPECT_TRUE(host_aa_->test_app_shim_
                  ->request_notification_permission_callback_.Wait());
  host_aa_->test_app_shim_->request_notification_permission_callback_.Take()
      .Run(mac_notifications::mojom::RequestPermissionResult::
               kPermissionPreviouslyGranted);
  EXPECT_EQ(mac_notifications::mojom::RequestPermissionResult::
                kPermissionPreviouslyGranted,
            result.Get());
}

TEST_F(AppShimManagerTest, RequestNotificationPermissionWithoutAppRunning) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(&profile_a_));

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kBackground));

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());
  EXPECT_TRUE(host_aa_->test_app_shim_
                  ->request_notification_permission_callback_.Wait());
  host_aa_->test_app_shim_->request_notification_permission_callback_.Take()
      .Run(mac_notifications::mojom::RequestPermissionResult::
               kPermissionPreviouslyDenied);
  EXPECT_EQ(mac_notifications::mojom::RequestPermissionResult::
                kPermissionPreviouslyDenied,
            result.Get());
}

TEST_F(AppShimManagerTest,
       RequestNotificationPermissionWithoutAppRunningAndBrowserClosing) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(&profile_a_));

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kBackground));

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());

  EXPECT_TRUE(host_aa_->test_app_shim_
                  ->request_notification_permission_callback_.Wait());

  // Pretend the last browser for this app/profile was just closed, and the
  // profile has been unloaded as a result of that.
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*delegate_, AppIsInstalled(nullptr, kTestAppIdA))
      .WillRepeatedly(Return(false));

  // Now have the app shim connect to the browser process.
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);

  host_aa_->test_app_shim_->request_notification_permission_callback_.Take()
      .Run(mac_notifications::mojom::RequestPermissionResult::
               kPermissionPreviouslyDenied);
  EXPECT_EQ(mac_notifications::mojom::RequestPermissionResult::
                kPermissionPreviouslyDenied,
            result.Get());
}

TEST_F(AppShimManagerTest,
       AppShimFailToConnectForNotificationPermissionAfterBrowserClosed) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(&profile_a_));

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kBackground));

  // Capture the terminated callback so we can simulate the app shim failing
  // to launch.
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());

  EXPECT_TRUE(host_aa_->test_app_shim_
                  ->request_notification_permission_callback_.Wait());

  // Pretend the last browser for this app/profile was just closed, and the
  // profile has been unloaded as a result of that.
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*delegate_, AppIsInstalled(nullptr, kTestAppIdA))
      .WillRepeatedly(Return(false));

  // Report that the process terminated.
  ASSERT_TRUE(terminated_callback);
  std::move(terminated_callback).Run();
}

TEST_F(AppShimManagerTest,
       RequestNotificationPermissionWithAppShimFailingToLaunch) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // This app is installed for profile A throughout this test.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(&profile_a_));

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kBackground));

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());
  EXPECT_TRUE(host_aa_->test_app_shim_
                  ->request_notification_permission_callback_.Wait());

  // Simulate the app shim failing to launch (or otherwise terminating) by
  // dropping the callback.
  host_aa_->test_app_shim_->request_notification_permission_callback_.Take()
      .Reset();

  EXPECT_EQ(mac_notifications::mojom::RequestPermissionResult::kRequestFailed,
            result.Get());
}

TEST_F(AppShimManagerTest, RequestNotificationPermissionWithAppNotInstalled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  EXPECT_CALL(*manager_, ProfileForBackgroundShimLaunch(kTestAppIdA))
      .WillOnce(Return(nullptr));

  // Trigger a notification permission request.
  base::test::TestFuture<mac_notifications::mojom::RequestPermissionResult>
      result;
  manager_->ShowNotificationPermissionRequest(kTestAppIdA,
                                              result.GetCallback());
  EXPECT_EQ(mac_notifications::mojom::RequestPermissionResult::kRequestFailed,
            result.Get());
}

TEST_F(AppShimManagerTest, CachedNotificationPermissionStatus) {
  using PermissionStatus = mac_notifications::mojom::PermissionStatus;
  scoped_feature_list_.InitWithFeatures(
      {features::kAppShimNotificationAttribution}, {});

  // Create and launch shim for app A in profile A.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_,
              DoLaunchShim(&profile_a_, kTestAppIdA,
                           web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                           web_app::ShimLaunchMode::kNormal));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);

  // Initial cached status should be "not determined".
  EXPECT_EQ(PermissionStatus::kNotDetermined,
            AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(
                kTestAppIdA));

  // Trigger updates to the notification status.
  base::test::TestFuture<const std::string&> app_changed;
  auto app_changed_registration =
      AppShimRegistry::Get()->RegisterAppChangedCallback(
          app_changed.GetRepeatingCallback());

  for (auto status :
       {PermissionStatus::kGranted, PermissionStatus::kNotDetermined,
        PermissionStatus::kPromptPending, PermissionStatus::kDenied}) {
    host_aa_->NotificationPermissionStatusChanged(status);
    EXPECT_EQ(kTestAppIdA, app_changed.Take());
    EXPECT_EQ(status,
              AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(
                  kTestAppIdA));
  }
}

}  // namespace apps
