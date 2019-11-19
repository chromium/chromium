// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"

#include <unistd.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/platform_apps/app_shim_registry_mac.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using extensions::Extension;
typedef extensions::AppWindowRegistry::AppWindowList AppWindowList;

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

class MockDelegate : public ExtensionAppShimHandler::Delegate {
 public:
  virtual ~MockDelegate() {}

  void GetProfilesForAppAsync(
      const std::string& app_id,
      const std::vector<base::FilePath>& profile_paths_to_check,
      base::OnceCallback<void(const std::vector<base::FilePath>&)> callback)
      override {
    get_profiles_for_app_callbacks_.push_back(
        base::BindOnce(std::move(callback), profile_paths_to_check));
  }

  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath&));
  void LoadProfileAsync(const base::FilePath& path,
                        base::OnceCallback<void(Profile*)> callback) override {
    CaptureLoadProfileCallback(path, std::move(callback));
  }
  MOCK_METHOD1(IsProfileLockedForPath, bool(const base::FilePath&));

  MOCK_METHOD2(GetWindows, AppWindowList(Profile*, const std::string&));

  MOCK_METHOD2(MaybeGetAppExtension,
               const Extension*(content::BrowserContext*, const std::string&));
  // Note that DoEnableExtension takes |callback| as a reference.
  void EnableExtension(Profile* profile,
                       const std::string& app_id,
                       base::OnceCallback<void()> callback) {
    DoEnableExtension(profile, app_id, callback);
  }
  MOCK_METHOD3(DoEnableExtension,
               void(Profile*, const std::string&, base::OnceCallback<void()>&));
  MOCK_METHOD3(LaunchApp,
               void(Profile*,
                    const Extension*,
                    const std::vector<base::FilePath>&));

  // Conditionally mock LaunchShim. Some tests will execute |launch_callback|
  // with a particular value.
  MOCK_METHOD3(DoLaunchShim, void(Profile*, const Extension*, bool));
  void LaunchShim(Profile* profile,
                  const Extension* extension,
                  bool recreate_shim,
                  ShimLaunchedCallback launched_callback,
                  ShimTerminatedCallback terminated_callback) override {
    if (launch_shim_callback_capture_)
      *launch_shim_callback_capture_ = std::move(launched_callback);
    if (terminated_shim_callback_capture_)
      *terminated_shim_callback_capture_ = std::move(terminated_callback);
    DoLaunchShim(profile, extension, recreate_shim);
  }
  void SetCaptureShimLaunchedCallback(ShimLaunchedCallback* callback) {
    launch_shim_callback_capture_ = callback;
  }
  void SetCaptureShimTerminatedCallback(ShimTerminatedCallback* callback) {
    terminated_shim_callback_capture_ = callback;
  }

  MOCK_METHOD0(LaunchUserManager, void());

  MOCK_METHOD0(MaybeTerminate, void());

  void SetAllowShimToConnect(bool should_create_host) {
    allow_shim_to_connect_ = should_create_host;
  }
  bool AllowShimToConnect(Profile* profile,
                          const extensions::Extension* extension) override {
    return allow_shim_to_connect_;
  }

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

  void CaptureLoadProfileCallback(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback) {
    callbacks_[path] = std::move(callback);
  }

  bool RunLoadProfileCallback(
      const base::FilePath& path,
      Profile* profile) {
    std::move(callbacks_[path]).Run(profile);
    return callbacks_.erase(path);
  }

  bool RunGetProfilesForAppCallback() {
    if (get_profiles_for_app_callbacks_.empty())
      return false;
    std::move(get_profiles_for_app_callbacks_.front()).Run();
    get_profiles_for_app_callbacks_.pop_front();
    return true;
  }

 private:
  ShimLaunchedCallback* launch_shim_callback_capture_ = nullptr;
  ShimTerminatedCallback* terminated_shim_callback_capture_ = nullptr;
  std::map<base::FilePath, base::OnceCallback<void(Profile*)>> callbacks_;
  std::unique_ptr<AppShimHost> host_for_create_ = nullptr;

  std::list<base::OnceClosure> get_profiles_for_app_callbacks_;
  bool allow_shim_to_connect_ = true;
};

class TestingExtensionAppShimHandler : public ExtensionAppShimHandler {
 public:
  TestingExtensionAppShimHandler(Delegate* delegate) {
    set_delegate(delegate);
  }
  virtual ~TestingExtensionAppShimHandler() {}

  MOCK_METHOD3(OnShimFocus,
               void(AppShimHost* host,
                    AppShimFocusType,
                    const std::vector<base::FilePath>& files));

  void RealOnShimFocus(AppShimHost* host,
                       AppShimFocusType focus_type,
                       const std::vector<base::FilePath>& files) {
    ExtensionAppShimHandler::OnShimFocus(host, focus_type, files);
  }

  void SetProfileMenuItems(
      std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items) {
    new_profile_menu_items_ = std::move(new_profile_menu_items);
    OnAvatarMenuChanged(nullptr);
  }
  void UpdateProfileMenuItems() override {
    profile_menu_items_.clear();
    for (const auto& item : new_profile_menu_items_)
      profile_menu_items_.push_back(item.Clone());
  }

  void SetAcceptablyCodeSigned(bool is_acceptable_code_signed) {
    is_acceptably_code_signed_ = is_acceptable_code_signed;
  }
  bool IsAcceptablyCodeSigned(pid_t pid) const override {
    return is_acceptably_code_signed_;
  }

  content::NotificationRegistrar& GetRegistrar() { return registrar(); }

 private:
  std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items_;
  bool is_acceptably_code_signed_ = true;
  DISALLOW_COPY_AND_ASSIGN(TestingExtensionAppShimHandler);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestingAppShimHostBootstrap(
      const base::FilePath& profile_path,
      const std::string& app_id,
      bool is_from_bookmark,
      base::Optional<apps::AppShimLaunchResult>* launch_result)
      : AppShimHostBootstrap(getpid()),
        profile_path_(profile_path),
        app_id_(app_id),
        is_from_bookmark_(is_from_bookmark),
        launch_result_(launch_result),
        weak_factory_(this) {}
  void DoTestLaunch(apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    mojo::Remote<chrome::mojom::AppShimHost> host;
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = profile_path_;
    app_shim_info->app_id = app_id_;
    if (is_from_bookmark_)
      app_shim_info->app_url = GURL("https://example.com");
    app_shim_info->launch_type = launch_type;
    app_shim_info->files = files;
    OnShimConnected(
        host.BindNewPipeAndPassReceiver(), std::move(app_shim_info),
        base::BindOnce(&TestingAppShimHostBootstrap::DoTestLaunchDone,
                       launch_result_));
  }

  static void DoTestLaunchDone(
      base::Optional<apps::AppShimLaunchResult>* launch_result,
      apps::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    if (launch_result)
      launch_result->emplace(result);
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
  base::Optional<apps::AppShimLaunchResult>* launch_result_;
  base::WeakPtrFactory<TestingAppShimHostBootstrap> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class TestAppShim : public chrome::mojom::AppShim {
 public:
  // chrome::mojom::AppShim:
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}
  void SetUserAttention(apps::AppShimAttentionType attention_type) override {}
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override {
    profile_menu_items_ = std::move(profile_menu_items);
  }

  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;
};

class TestHost : public AppShimHost {
 public:
  TestHost(const base::FilePath& profile_path,
           const std::string& app_id,
           TestingExtensionAppShimHandler* handler)
      : AppShimHost(handler,
                    app_id,
                    profile_path,
                    false /* uses_remote_views */),
        test_app_shim_(new TestAppShim),
        test_weak_factory_(this) {}
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

  using AppShimHost::ProfileSelectedFromMenu;

  std::unique_ptr<TestAppShim> test_app_shim_;

 private:
  bool did_connect_to_host_ = false;

  base::WeakPtrFactory<TestHost> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestHost);
};

class ExtensionAppShimHandlerTestBase : public testing::Test {
 protected:
  ExtensionAppShimHandlerTestBase() {}
  ~ExtensionAppShimHandlerTestBase() override {}

  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    AppShimRegistry::Get()->RegisterLocalPrefs(local_state_->registry());
    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        local_state_.get(), base::FilePath("/User/Data/Dir/"));

    delegate_ = new MockDelegate;
    handler_.reset(new TestingExtensionAppShimHandler(delegate_));
    profile_path_a_ = base::FilePath("/User/Data/Dir/Profile A");
    profile_path_b_ = base::FilePath("/User/Data/Dir/Profile B");
    AppShimHostBootstrap::SetClient(handler_.get());
    bootstrap_aa_ = (new TestingAppShimHostBootstrap(
                         profile_path_a_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_aa_result_))
                        ->GetWeakPtr();
    bootstrap_ba_ = (new TestingAppShimHostBootstrap(
                         profile_path_b_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_ba_result_))
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
                                                 handler_.get());
    host_ab_unique_ = std::make_unique<TestHost>(profile_path_a_, kTestAppIdB,
                                                 handler_.get());
    host_ba_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdA,
                                                 handler_.get());
    host_bb_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdB,
                                                 handler_.get());
    host_aa_duplicate_unique_ = std::make_unique<TestHost>(
        profile_path_a_, kTestAppIdA, handler_.get());

    host_aa_ = host_aa_unique_->GetWeakPtr();
    host_ab_ = host_ab_unique_->GetWeakPtr();
    host_ba_ = host_ba_unique_->GetWeakPtr();
    host_bb_ = host_bb_unique_->GetWeakPtr();

    base::FilePath extension_path("/fake/path");
    extension_a_ =
        extensions::ExtensionBuilder("Fake Name")
            .SetLocation(extensions::Manifest::INTERNAL)
            .SetPath(extension_path)
            .SetID(kTestAppIdA)
            .AddFlags(extensions::Extension::InitFromValueFlags::FROM_BOOKMARK)
            .Build();

    extension_b_ = extensions::ExtensionBuilder("Fake Name")
                       .SetLocation(extensions::Manifest::INTERNAL)
                       .SetPath(extension_path)
                       .SetID(kTestAppIdB)
                       .Build();

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
      handler_->SetProfileMenuItems(std::move(items));
    }

    EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_a_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));
    EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_b_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));

    // In most tests, we don't care about the result of GetWindows, it just
    // needs to be non-empty.
    AppWindowList app_window_list;
    app_window_list.push_back(static_cast<extensions::AppWindow*>(NULL));
    EXPECT_CALL(*delegate_, GetWindows(_, _))
        .WillRepeatedly(Return(app_window_list));

    EXPECT_CALL(*delegate_, MaybeGetAppExtension(_, kTestAppIdA))
        .WillRepeatedly(Return(extension_a_.get()));
    EXPECT_CALL(*delegate_, MaybeGetAppExtension(_, kTestAppIdB))
        .WillRepeatedly(Return(extension_b_.get()));
    EXPECT_CALL(*delegate_, LaunchApp(_, _, _))
        .WillRepeatedly(Return());
  }

  void TearDown() override {
    host_aa_unique_.reset();
    host_ab_unique_.reset();
    host_bb_unique_.reset();
    host_aa_duplicate_unique_.reset();
    delegate_->SetHostForCreate(nullptr);
    handler_.reset();

    // Delete the bootstraps via their weak pointers if they haven't been
    // deleted yet. Note that this must be done after the profiles and hosts
    // have been destroyed (because they may now own the bootstraps).
    delete bootstrap_aa_.get();
    delete bootstrap_ba_.get();
    delete bootstrap_xa_.get();
    delete bootstrap_ab_.get();
    delete bootstrap_bb_.get();
    delete bootstrap_aa_duplicate_.get();
    delete bootstrap_aa_thethird_.get();

    AppShimHostBootstrap::SetClient(nullptr);

    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        nullptr, base::FilePath());
  }

  void DoShimLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    std::unique_ptr<TestHost> host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    if (host)
      delegate_->SetHostForCreate(std::move(host));
    bootstrap->DoTestLaunch(launch_type, files);
    EXPECT_TRUE(delegate_->RunGetProfilesForAppCallback());
  }

  void NormalLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host), APP_SHIM_LAUNCH_NORMAL,
                 std::vector<base::FilePath>());
  }

  void RegisterOnlyLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                          std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host), APP_SHIM_LAUNCH_REGISTER_ONLY,
                 std::vector<base::FilePath>());
  }

  // Completely launch a shim host and leave it running.
  void LaunchAndActivate(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                         std::unique_ptr<TestHost> host_unique,
                         Profile* profile) {
    base::WeakPtr<TestHost> host = host_unique->GetWeakPtr();
    NormalLaunch(bootstrap, std::move(host_unique));
    EXPECT_EQ(host.get(), handler_->FindHost(profile, host->GetAppId()));
    EXPECT_CALL(*handler_, OnShimFocus(host.get(), APP_SHIM_FOCUS_NORMAL, _));
    handler_->OnAppActivated(profile, host->GetAppId());
    EXPECT_TRUE(host->did_connect_to_host());
  }

  // Simulates a focus request coming from a running app shim.
  void ShimNormalFocus(TestHost* host) {
    EXPECT_CALL(*handler_, OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, _))
        .WillOnce(Invoke(handler_.get(),
                         &TestingExtensionAppShimHandler::RealOnShimFocus));

    const std::vector<base::FilePath> no_files;
    handler_->OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, no_files);
  }

  content::BrowserTaskEnvironment task_environment_;
  MockDelegate* delegate_;
  std::unique_ptr<TestingExtensionAppShimHandler> handler_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;

  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ba_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_xa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ab_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_bb_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_duplicate_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_thethird_;

  base::Optional<apps::AppShimLaunchResult> bootstrap_aa_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_ba_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_xa_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_ab_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_bb_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_aa_duplicate_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_aa_thethird_result_;

  // Unique ptr to the TestsHosts used by the tests. These are passed by
  // std::move durnig tests. To access them after they have been passed, use
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

  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;

 private:
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandlerTestBase);
};

class ExtensionAppShimHandlerTest : public ExtensionAppShimHandlerTestBase {
 public:
  void SetUp() override {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAppShimMultiProfile});
    ExtensionAppShimHandlerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

class ExtensionAppShimHandlerTestMultiProfile
    : public ExtensionAppShimHandlerTestBase {
 public:
  void SetUp() override {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{features::kAppShimMultiProfile},
        /*disabled_features=*/{});
    ExtensionAppShimHandlerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(ExtensionAppShimHandlerTest, LaunchProfileNotFound) {
  // Bad profile path.
  EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(static_cast<Profile*>(nullptr)));
  NormalLaunch(bootstrap_aa_, nullptr);
  delegate_->RunLoadProfileCallback(profile_path_a_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchProfileIsLocked) {
  // Profile is locked.
  EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_a_))
      .WillOnce(Return(true));
  EXPECT_CALL(*delegate_, LaunchUserManager());
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_PROFILE_LOCKED, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAppNotFound) {
  // App not found.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(static_cast<const Extension*>(NULL)));
  EXPECT_CALL(*delegate_, DoEnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(APP_SHIM_LAUNCH_APP_NOT_FOUND, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAppNotEnabled) {
  // App not found.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillOnce(Return(static_cast<const Extension*>(NULL)))
      .WillRepeatedly(Return(extension_a_.get()));
  EXPECT_CALL(*delegate_, DoEnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAndCloseShim) {
  // Normal startup.
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(host_ab_.get(), handler_->FindHost(&profile_a_, kTestAppIdB));

  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, extension_b_.get(), some_file));
  DoShimLaunch(bootstrap_bb_, std::move(host_bb_unique_),
               APP_SHIM_LAUNCH_NORMAL, some_file);
  EXPECT_EQ(host_bb_.get(), handler_->FindHost(&profile_b_, kTestAppIdB));

  // Activation when there is a registered shim finishes launch with success and
  // focuses the app.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_NORMAL, _));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);

  // Starting and closing a second host just focuses the original host of the
  // app.
  EXPECT_CALL(*handler_,
              OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, some_file));

  DoShimLaunch(bootstrap_aa_duplicate_, std::move(host_aa_duplicate_unique_),
               APP_SHIM_LAUNCH_NORMAL, some_file);
  EXPECT_EQ(APP_SHIM_LAUNCH_DUPLICATE_HOST, *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  handler_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_EQ(host_aa_.get(), nullptr);
}

TEST_F(ExtensionAppShimHandlerTest, AppLifetime) {
  // When the app activates, a host is created. If there is no shim, one is
  // launched.
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), false));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal shim launch adds an entry in the map.
  // App should not be launched here, but return success to the shim.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Return no app windows for OnShimFocus.
  AppWindowList app_window_list;
  EXPECT_CALL(*delegate_, GetWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(app_window_list));

  // Non-reopen focus does nothing.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  ShimNormalFocus(host_aa_.get());

  // Reopen focus launches the app.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, _))
      .WillOnce(Invoke(handler_.get(),
                       &TestingExtensionAppShimHandler::RealOnShimFocus));
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), some_file));
  handler_->OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, some_file);

  // Process disconnect will cause the host to be deleted.
  handler_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());

  // OnAppDeactivated should trigger a MaybeTerminate call.
  EXPECT_CALL(*delegate_, MaybeTerminate())
      .WillOnce(Return());
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdA);
}

TEST_F(ExtensionAppShimHandlerTest, FailToLaunch) {
  // When the app activates, it requests a launch.
  ShimLaunchedCallback launch_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launch_callback);
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), false));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launch_callback);

  // Run the callback claiming that the launch failed. This should trigger
  // another launch, this time forcing shim recreation.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), true));
  std::move(launch_callback).Run(base::Process());
  EXPECT_TRUE(launch_callback);

  // Report that the launch failed. This should trigger deletion of the host.
  EXPECT_NE(nullptr, host_aa_.get());
  std::move(launch_callback).Run(base::Process());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(ExtensionAppShimHandlerTest, FailToConnect) {
  // When the app activates, it requests a launch.
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), false));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Report that the process terminated. This should trigger a re-create and
  // re-launch.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), true));
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

TEST_F(ExtensionAppShimHandlerTest, FailCodeSignature) {
  handler_->SetAcceptablyCodeSigned(false);
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Fail to code-sign. This should result in a host being created, and a launch
  // having been requested.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), false));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
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
  handler_->SetAcceptablyCodeSigned(true);
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), true));
  std::move(terminated_callback).Run();
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  RegisterOnlyLaunch(bootstrap_aa_thethird_, std::move(host_aa_unique_));
  EXPECT_TRUE(host_aa_->HasBootstrapConnected());
}

TEST_F(ExtensionAppShimHandlerTest, MaybeTerminate) {
  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), handler_->FindHost(&profile_a_, kTestAppIdB));

  // Return empty window list.
  AppWindowList app_window_list;
  EXPECT_CALL(*delegate_, GetWindows(_, _))
      .WillRepeatedly(Return(app_window_list));

  // Quitting when there's another shim should not terminate.
  EXPECT_CALL(*delegate_, MaybeTerminate())
      .Times(0);
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim should terminate.
  EXPECT_CALL(*delegate_, MaybeTerminate());
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(ExtensionAppShimHandlerTest, RegisterOnly) {
  // For an APP_SHIM_LAUNCH_REGISTER_ONLY, don't launch the app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_TRUE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Close the shim, removing the entry in the map.
  handler_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTest, DontCreateHost) {
  delegate_->SetAllowShimToConnect(false);

  // The app should be launched.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).Times(1);
  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  // But the bootstrap should be closed.
  EXPECT_EQ(APP_SHIM_LAUNCH_DUPLICATE_HOST, *bootstrap_ab_result_);
  // And we should create no host.
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdB));
}

TEST_F(ExtensionAppShimHandlerTest, LoadProfile) {
  // If the profile is not loaded when an OnShimProcessConnected arrives, return
  // false and load the profile asynchronously. Launch the app when the profile
  // is ready.
  EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
      .WillOnce(Return(static_cast<Profile*>(NULL)))
      .WillRepeatedly(Return(&profile_a_));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
  delegate_->RunLoadProfileCallback(profile_path_a_, &profile_a_);
  EXPECT_TRUE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

// Tests that calls to OnShimFocus, OnShimHide correctly handle a null extension
// being provided by the extension system.
TEST_F(ExtensionAppShimHandlerTest, ExtensionUninstalled) {
  LaunchAndActivate(bootstrap_aa_, std::move(host_aa_unique_), &profile_a_);

  // Have GetWindows() return an empty window list for focus (otherwise, it
  // will contain a single nullptr, which can't be focused). Expect 1 call only.
  AppWindowList empty_window_list;
  EXPECT_CALL(*delegate_, GetWindows(_, _)).WillOnce(Return(empty_window_list));

  ShimNormalFocus(host_aa_.get());
  EXPECT_NE(nullptr, host_aa_.get());

  // Set up the mock to return a null extension, as if it were uninstalled.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(nullptr));

  // Now trying to focus should automatically close the shim, and not try to
  // get the window list.
  ShimNormalFocus(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(ExtensionAppShimHandlerTest, PreExistingHost) {
  // Create a host for our profile.
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for this host. It should find the pre-existing host, and the
  // pre-existing host's launch result should be set.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_NORMAL, _))
      .Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  EXPECT_FALSE(host_aa_->did_connect_to_host());
  DoShimLaunch(bootstrap_aa_, nullptr, APP_SHIM_LAUNCH_REGISTER_ONLY,
               std::vector<base::FilePath>());
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Try to launch the app again. It should fail to launch, and the previous
  // profile should remain.
  DoShimLaunch(bootstrap_aa_duplicate_, nullptr, APP_SHIM_LAUNCH_REGISTER_ONLY,
               std::vector<base::FilePath>());
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(APP_SHIM_LAUNCH_DUPLICATE_HOST, *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, MultiProfile) {
  // Test with a bookmark app (host is shared).
  {
    // Create a host for profile A.
    delegate_->SetHostForCreate(std::move(host_aa_unique_));
    EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdA));
    handler_->OnAppActivated(&profile_a_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());

    // Ensure that profile B has the same host.
    delegate_->SetHostForCreate(std::move(host_ba_unique_));
    EXPECT_EQ(nullptr, handler_->FindHost(&profile_b_, kTestAppIdA));
    handler_->OnAppActivated(&profile_b_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_b_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());
  }

  // Test with a non-bookmark app (host is not shared).
  {
    // Create a host for profile A.
    delegate_->SetHostForCreate(std::move(host_ab_unique_));
    EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdB));
    handler_->OnAppActivated(&profile_a_, kTestAppIdB);
    EXPECT_EQ(host_ab_.get(), handler_->FindHost(&profile_a_, kTestAppIdB));
    EXPECT_FALSE(host_ab_->did_connect_to_host());

    // Ensure that profile B has the same host.
    delegate_->SetHostForCreate(std::move(host_bb_unique_));
    EXPECT_EQ(nullptr, handler_->FindHost(&profile_b_, kTestAppIdB));
    handler_->OnAppActivated(&profile_b_, kTestAppIdB);
    EXPECT_EQ(host_bb_.get(), handler_->FindHost(&profile_b_, kTestAppIdB));
    EXPECT_FALSE(host_bb_->did_connect_to_host());
  }
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, MultiProfileShimLaunch) {
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(),
                                       false /* recreate_shim */));
  EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for profile B. This should not cause a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _)).Times(0);
  handler_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, MultiProfileSelectMenu) {
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(),
                                       false /* recreate_shim */));
  EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Select profile B from the menu. This should request that the app be
  // launched.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, extension_a_.get(), _));
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _)).Times(0);
  handler_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Select profile A and B from the menu -- this should not request a launch,
  // because the profiles are already enabled.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).Times(0);
  host_aa_->ProfileSelectedFromMenu(profile_path_a_);
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, ProfileMenuOneProfile) {
  // Set this app to be installed for profile A.
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    handler_->SetProfileMenuItems(std::move(items));
  }

  // When the app activates, a host is created. This will trigger building
  // the avatar menu.
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, extension_a_.get(), false));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_TRUE(delegate_->RunGetProfilesForAppCallback());
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Launch the shim.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  const auto& menu_items = host_aa_->test_app_shim_->profile_menu_items_;

  // We should have no menu items, because there is only one installed profile.
  EXPECT_FALSE(delegate_->RunGetProfilesForAppCallback());
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
    handler_->SetProfileMenuItems(std::move(items));
  }
  EXPECT_TRUE(delegate_->RunGetProfilesForAppCallback());
  EXPECT_FALSE(delegate_->RunGetProfilesForAppCallback());

  // We should now have 2 menu items. They should be sorted by menu_index,
  // making b be before a.
  EXPECT_EQ(menu_items.size(), 2u);
  EXPECT_EQ(menu_items[0]->profile_path, profile_path_b_);
  EXPECT_EQ(menu_items[1]->profile_path, profile_path_a_);

  // Activate profile B. This should trigger re-building the avatar menu.
  handler_->OnAppActivated(&profile_b_, kTestAppIdA);
  EXPECT_TRUE(delegate_->RunGetProfilesForAppCallback());
  EXPECT_FALSE(delegate_->RunGetProfilesForAppCallback());
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, FindProfileFromBadProfile) {
  // Set this app to be installed for profile A.
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    handler_->SetProfileMenuItems(std::move(items));
  }

  // Launch the shim requesting profile B.
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, extension_a_.get(), _))
      .Times(0);
  NormalLaunch(bootstrap_ba_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_ba_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTestMultiProfile, FindProfileFromNoProfile) {
  // Set this app to be installed for profile A.
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    handler_->SetProfileMenuItems(std::move(items));
  }

  // Launch the shim without specifying a profile.
  delegate_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, extension_a_.get(), _))
      .Times(0);
  NormalLaunch(bootstrap_xa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_xa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
}
}  // namespace apps
