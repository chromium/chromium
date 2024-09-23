// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "components/services/app_service/public/cpp/types_util.h"
#endif

namespace apps {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class FakePublisherForProxyTest : public AppPublisher {
 public:
  FakePublisherForProxyTest(AppServiceProxy* proxy,
                            AppType app_type,
                            std::vector<std::string> initial_app_ids)
      : AppPublisher(proxy),
        app_type_(app_type),
        known_app_ids_(std::move(initial_app_ids)) {
    RegisterPublisher(app_type_);
    CallOnApps(known_app_ids_, /*uninstall=*/false);
  }

  FakePublisherForProxyTest(AppServiceProxy* proxy, AppType app_type)
      : AppPublisher(proxy), app_type_(app_type) {
    RegisterPublisher(app_type_);
  }

  void InitApps(std::vector<std::string> initial_app_ids) {
    known_app_ids_ = std::move(initial_app_ids);
    CallOnApps(known_app_ids_, /*uninstall=*/false);
  }

  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override {}

  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths) override {}

  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override {}

  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override {}

  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override {}

  void UninstallApps(std::vector<std::string> app_ids) {
    CallOnApps(app_ids, /*uninstall=*/true);

    for (const auto& app_id : app_ids) {
      known_app_ids_.push_back(app_id);
    }
  }

  bool AppHasSupportedLinksPreference(const std::string& app_id) {
    return supported_link_apps_.find(app_id) != supported_link_apps_.end();
  }

 private:
  void CallOnApps(std::vector<std::string>& app_ids, bool uninstall) {
    std::vector<AppPtr> apps;
    for (const auto& app_id : app_ids) {
      auto app = std::make_unique<App>(app_type_, app_id);
      app->readiness =
          uninstall ? Readiness::kUninstalledByUser : Readiness::kReady;
      apps.push_back(std::move(app));
    }
    AppPublisher::Publish(std::move(apps), app_type_,
                          /*should_notify_initialized=*/true);
  }

  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override {
    if (open_in_app) {
      supported_link_apps_.insert(app_id);
    } else {
      supported_link_apps_.erase(app_id);
    }
  }

  AppType app_type_;
  std::vector<std::string> known_app_ids_;
  std::set<std::string> supported_link_apps_;
};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// FakeAppRegistryCacheObserver is used to test OnAppUpdate.
class FakeAppRegistryCacheObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit FakeAppRegistryCacheObserver(apps::AppRegistryCache* cache) {
    app_registry_cache_observer_.Observe(cache);
  }

  ~FakeAppRegistryCacheObserver() override = default;

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    if (base::Contains(app_ids_, update.AppId())) {
      app_ids_.erase(update.AppId());
    }
    if (app_ids_.empty() && !result_.IsReady()) {
      result_.SetValue();
    }
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  void WaitForOnAppUpdate(const std::set<std::string>& app_ids) {
    app_ids_ = app_ids;
    EXPECT_TRUE(result_.Wait());
  }

 private:
  base::test::TestFuture<void> result_;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  std::set<std::string> app_ids_;
};

class FakeSubscriberForProxyTest : public SubscriberCrosapi {
 public:
  explicit FakeSubscriberForProxyTest(Profile* profile)
      : SubscriberCrosapi(profile) {
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->RegisterCrosApiSubScriber(this);
  }

  PreferredAppsList& preferred_apps_list() { return preferred_apps_list_; }

  void OnPreferredAppsChanged(PreferredAppChangesPtr changes) override {
    preferred_apps_list_.ApplyBulkUpdate(std::move(changes));
  }

  void InitializePreferredApps(apps::PreferredApps preferred_apps) override {
    preferred_apps_list_.Init(std::move(preferred_apps));
  }

 private:
  apps::PreferredAppsList preferred_apps_list_;
};
#endif

class AppServiceProxyTest : public testing::Test {
 public:
  AppServiceProxyTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
  }

 protected:
  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;

  class FakeIconLoader : public apps::IconLoader {
   public:
    void FlushPendingCallbacks() {
      for (auto& callback : pending_callbacks_) {
        auto iv = std::make_unique<IconValue>();
        iv->icon_type = IconType::kUncompressed;
        iv->uncompressed =
            gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
        iv->is_placeholder_icon = false;

        std::move(callback).Run(std::move(iv));
        num_inner_finished_callbacks_++;
      }
      pending_callbacks_.clear();
    }

    int NumInnerFinishedCallbacks() { return num_inner_finished_callbacks_; }
    int NumPendingCallbacks() { return pending_callbacks_.size(); }

   private:
    std::unique_ptr<Releaser> LoadIconFromIconKey(
        const std::string& id,
        const IconKey& icon_key,
        IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::LoadIconCallback callback) override {
      if (icon_type == IconType::kUncompressed) {
        pending_callbacks_.push_back(std::move(callback));
      }
      return nullptr;
    }

    int num_inner_finished_callbacks_ = 0;
    std::vector<apps::LoadIconCallback> pending_callbacks_;
  };

  void OverrideAppServiceProxyInnerIconLoader(AppServiceProxy* proxy,
                                              apps::IconLoader* icon_loader) {
    proxy->OverrideInnerIconLoaderForTesting(icon_loader);
  }

  Profile* profile() { return profile_.get(); }

  AppServiceProxy* proxy() { return app_service_proxy_; }

  int NumOuterFinishedCallbacks() { return num_outer_finished_callbacks_; }

  int num_outer_finished_callbacks_ = 0;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AppServiceProxy> app_service_proxy_;
};

class AppServiceProxyIconTest : public AppServiceProxyTest {
 protected:
  UniqueReleaser LoadIcon(apps::AppServiceProxy* proxy,
                          const std::string& app_id) {
    return proxy->LoadIcon(
        app_id, IconType::kUncompressed, /*size_hint_in_dip=*/1,
        /*allow_placeholder_icon=*/false,
        base::BindOnce([](int* num_callbacks,
                          apps::IconValuePtr icon) { ++(*num_callbacks); },
                       &num_outer_finished_callbacks_));
  }
};

TEST_F(AppServiceProxyIconTest, IconCache) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCache code, see icon_cache_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCache but also other IconLoader filters, such as an IconCoalescer.
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(proxy(), &fake);

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c0 = LoadIcon(proxy(), "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(1, NumOuterFinishedCallbacks());

  // The next LoadIcon call should be a cache hit.
  UniqueReleaser c1 = LoadIcon(proxy(), "cromulent");
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // Destroy the IconLoader::Releaser's, clearing the cache.
  c0.reset();
  c1.reset();

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c2 = LoadIcon(proxy(), "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(3, NumOuterFinishedCallbacks());
}

TEST_F(AppServiceProxyIconTest, IconCoalescer) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCoalescer code, see icon_coalescer_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCoalescer but also other IconLoader filters, such as an IconCache.
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(proxy(), &fake);

  // Issue 4 LoadIcon requests, 2 after de-duplication.
  UniqueReleaser a0 = LoadIcon(proxy(), "avocet");
  UniqueReleaser a1 = LoadIcon(proxy(), "avocet");
  UniqueReleaser b2 = LoadIcon(proxy(), "brolga");
  UniqueReleaser a3 = LoadIcon(proxy(), "avocet");
  EXPECT_EQ(2, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // Resolve their responses.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Issue another request, that triggers neither IconCache nor IconCoalescer.
  UniqueReleaser c4 = LoadIcon(proxy(), "curlew");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Destroying the IconLoader::Releaser shouldn't affect the fact that there's
  // an in-flight "curlew" request to the FakeIconLoader.
  c4.reset();
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Issuing another "curlew" request should coalesce with the in-flight one.
  UniqueReleaser c5 = LoadIcon(proxy(), "curlew");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Resolving the in-flight request to the inner IconLoader, |fake|, should
  // resolve the two coalesced requests to the outer IconLoader, |proxy|.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(3, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(6, NumOuterFinishedCallbacks());
}

TEST_F(AppServiceProxyTest, ProxyAccessPerProfile) {
  TestingProfile::Builder profile_builder;

  // We expect an App Service in a regular profile.
  auto profile = profile_builder.Build();
  EXPECT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile.get()));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile.get());
  EXPECT_TRUE(proxy);

  // We expect App Service to be unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_FALSE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      incognito_profile));

  // But if it's accidentally called, we expect the same App Service in the
  // incognito profile branched from that regular profile.
  // TODO(crbug.com/40146603): this should be nullptr once we address all
  // incognito access to the App Service.
  auto* incognito_proxy =
      apps::AppServiceProxyFactory::GetForProfile(incognito_profile);
  EXPECT_EQ(proxy, incognito_proxy);

  // We expect a different App Service in the Guest Session profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // App service is not available for original profile.
  EXPECT_FALSE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      guest_profile.get()));

  // App service is available for OTR profile in Guest mode.
  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      guest_otr_profile));
  auto* guest_otr_proxy =
      apps::AppServiceProxyFactory::GetForProfile(guest_otr_profile);
  EXPECT_TRUE(guest_otr_proxy);
  EXPECT_NE(guest_otr_proxy, proxy);
#else
  EXPECT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      guest_profile.get()));
  auto* guest_proxy =
      apps::AppServiceProxyFactory::GetForProfile(guest_profile.get());
  EXPECT_TRUE(guest_proxy);
  EXPECT_NE(guest_proxy, proxy);
#endif
}

TEST_F(AppServiceProxyTest, ReinitializeClearsCache) {
  constexpr char kTestAppId[] = "pwa";
  {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, kTestAppId);
    apps.push_back(std::move(app));
    proxy()->OnApps(std::move(apps), AppType::kWeb,
                    /*should_notify_initialized=*/true);
  }

  EXPECT_EQ(proxy()->AppRegistryCache().GetAppType(kTestAppId), AppType::kWeb);

  proxy()->ReinitializeForTesting(proxy()->profile());

  EXPECT_EQ(proxy()->AppRegistryCache().GetAppType(kTestAppId),
            AppType::kUnknown);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class AppServiceProxyPreferredAppsTest : public AppServiceProxyTest {
 public:
  void SetUp() override {
    AppServiceProxyTest::SetUp();

    // Wait for the PreferredAppsList to be initialized from disk before tests
    // start modifying it.
    base::RunLoop file_read_run_loop;
    proxy()->ReinitializeForTesting(profile(),
                                    file_read_run_loop.QuitClosure());
    file_read_run_loop.Run();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  // Shortcut for adding apps to App Service without going through a real
  // Publisher.
  void OnApps(std::vector<AppPtr> apps, AppType type) {
    proxy()->OnApps(std::move(apps), type,
                    /*should_notify_initialized=*/false);
  }

  PreferredAppsList& GetPreferredAppsList() {
    return proxy()->preferred_apps_impl_->preferred_apps_list_;
  }

  PreferredAppsImpl* PreferredAppsImpl() {
    return proxy()->preferred_apps_impl_.get();
  }
};

TEST_F(AppServiceProxyPreferredAppsTest, UpdatedOnUninstall) {
  constexpr char kTestAppId[] = "foo";
  const GURL kTestUrl = GURL("https://www.example.com/");

  // Install an app and set it as preferred for a URL.
  {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, kTestAppId);
    app->readiness = Readiness::kReady;
    app->intent_filters.push_back(
        apps_util::MakeIntentFilterForUrlScope(kTestUrl));
    apps.push_back(std::move(app));

    OnApps(std::move(apps), AppType::kWeb);
    proxy()->SetSupportedLinksPreference(kTestAppId);

    std::optional<std::string> preferred_app =
        proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(kTestAppId, preferred_app);
  }

  // Updating the app should not change its preferred app status.
  {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, kTestAppId);
    app->last_launch_time = base::Time();
    apps.push_back(std::move(app));

    OnApps(std::move(apps), AppType::kWeb);

    std::optional<std::string> preferred_app =
        proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(kTestAppId, preferred_app);
  }

  // Uninstalling the app should remove it from the preferred app list.
  {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, kTestAppId);
    app->readiness = Readiness::kUninstalledByUser;
    apps.push_back(std::move(app));

    OnApps(std::move(apps), AppType::kWeb);

    std::optional<std::string> preferred_app =
        proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(std::nullopt, preferred_app);
  }
}

TEST_F(AppServiceProxyPreferredAppsTest, SetPreferredApp) {
  constexpr char kTestAppId1[] = "abc";
  constexpr char kTestAppId2[] = "def";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");

  auto url_filter_1 = apps_util::MakeIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::MakeIntentFilterForUrlScope(kTestUrl2);
  auto send_filter = apps_util::MakeIntentFilterForSend("image/png");

  std::vector<AppPtr> apps;
  AppPtr app1 = std::make_unique<App>(AppType::kWeb, kTestAppId1);
  app1->readiness = Readiness::kReady;
  app1->intent_filters.push_back(url_filter_1->Clone());
  app1->intent_filters.push_back(url_filter_2->Clone());
  app1->intent_filters.push_back(send_filter->Clone());
  apps.push_back(std::move(app1));

  AppPtr app2 = std::make_unique<App>(AppType::kWeb, kTestAppId2);
  app2->readiness = Readiness::kReady;
  app2->intent_filters.push_back(url_filter_1->Clone());
  apps.push_back(std::move(app2));

  OnApps(std::move(apps), AppType::kWeb);

  // Set app 1 as preferred. Both links should be set as preferred, but the
  // non-link filter is ignored.

  proxy()->SetSupportedLinksPreference(kTestAppId1);

  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));
  auto mime_intent = std::make_unique<Intent>(apps_util::kIntentActionSend);
  mime_intent->mime_type = "image/png";
  ASSERT_EQ(
      std::nullopt,
      proxy()->PreferredAppsList().FindPreferredAppForIntent(mime_intent));

  // Set app 2 as preferred. Both of the previous preferences for app 1 should
  // be removed.

  proxy()->SetSupportedLinksPreference(kTestAppId2);

  ASSERT_EQ(kTestAppId2,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(std::nullopt,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));

  // Remove all supported link preferences for app 2.

  proxy()->RemoveSupportedLinksPreference(kTestAppId2);

  ASSERT_EQ(std::nullopt,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
}

// Tests that writing a preferred app value before the PreferredAppsList is
// initialized queues the write for after initialization.
TEST_F(AppServiceProxyPreferredAppsTest, PreferredAppsWriteBeforeInit) {
  base::RunLoop run_loop_read;
  proxy()->ReinitializeForTesting(proxy()->profile(),
                                  run_loop_read.QuitClosure());
  GURL filter_url1("https://www.abc.com/");
  GURL filter_url2("https://www.def.com/");

  std::string kAppId1 = "aaa";
  std::string kAppId2 = "bbb";

  IntentFilters filters1;
  filters1.push_back(apps_util::MakeIntentFilterForUrlScope(filter_url1));
  proxy()->SetSupportedLinksPreference(kAppId1, std::move(filters1));

  IntentFilters filters2;
  filters2.push_back(apps_util::MakeIntentFilterForUrlScope(filter_url2));
  proxy()->SetSupportedLinksPreference(kAppId2, std::move(filters2));

  // Wait for the preferred apps list initialization to read from disk.
  run_loop_read.Run();

  // Both changes to the PreferredAppsList should have been applied.
  ASSERT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url1));
  ASSERT_EQ(kAppId2,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url2));
}

TEST_F(AppServiceProxyPreferredAppsTest, PreferredAppsPersistency) {
  const char kAppId1[] = "abcdefg";
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::MakeIntentFilterForUrlScope(filter_url);
  {
    base::RunLoop run_loop_read;
    base::RunLoop run_loop_write;
    proxy()->ReinitializeForTesting(proxy()->profile(),
                                    run_loop_read.QuitClosure(),
                                    run_loop_write.QuitClosure());
    run_loop_read.Run();
    IntentFilters filters;
    filters.push_back(apps_util::MakeIntentFilterForUrlScope(filter_url));
    proxy()->SetSupportedLinksPreference(kAppId1, std::move(filters));
    run_loop_write.Run();
  }
  // Create a new impl to initialize preferred apps from the disk.
  {
    base::RunLoop run_loop_read;
    proxy()->ReinitializeForTesting(proxy()->profile(),
                                    run_loop_read.QuitClosure());
    run_loop_read.Run();
    EXPECT_EQ(kAppId1,
              GetPreferredAppsList().FindPreferredAppForUrl(filter_url));
  }
}

TEST_F(AppServiceProxyPreferredAppsTest,
       PreferredAppsSetSupportedLinksPublisher) {
  GetPreferredAppsList().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";
  const char kAppId3[] = "opqrstu";

  auto intent_filter_a =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.a.com/"));
  auto intent_filter_b =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.b.com/"));
  auto intent_filter_c =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.c.com/"));

  FakePublisherForProxyTest pub(
      proxy(), AppType::kArc,
      std::vector<std::string>{kAppId1, kAppId2, kAppId3});

  IntentFilters app_1_filters;
  app_1_filters.push_back(intent_filter_a->Clone());
  app_1_filters.push_back(intent_filter_b->Clone());
  proxy()->SetSupportedLinksPreference(kAppId1, std::move(app_1_filters));

  IntentFilters app_2_filters;
  app_2_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId2, std::move(app_2_filters));

  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(kAppId1, GetPreferredAppsList().FindPreferredAppForUrl(
                         GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId1, GetPreferredAppsList().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId2, GetPreferredAppsList().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // App 3 overlaps with both App 1 and 2. Both previous apps should have all
  // their supported link filters removed.
  IntentFilters app_3_filters;
  app_3_filters.push_back(intent_filter_b->Clone());
  app_3_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId3, std::move(app_3_filters));

  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(std::nullopt, GetPreferredAppsList().FindPreferredAppForUrl(
                              GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId3, GetPreferredAppsList().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId3, GetPreferredAppsList().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // Setting App 3 as preferred again should not change anything.
  app_3_filters = std::vector<IntentFilterPtr>();
  app_3_filters.push_back(intent_filter_b->Clone());
  app_3_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId3, std::move(app_3_filters));

  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId3));

  proxy()->RemoveSupportedLinksPreference(kAppId3);

  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId3));
}

// Test that app with overlapped supported links works properly.
TEST_F(AppServiceProxyPreferredAppsTest, PreferredAppsOverlapSupportedLink) {
  // Test Initialize.
  GetPreferredAppsList().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";

  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");

  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(ConditionType::kScheme, filter_url_2.scheme(),
                               PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(ConditionType::kAuthority, filter_url_2.host(),
                               PatternMatchType::kLiteral, intent_filter_1);

  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(ConditionType::kScheme, filter_url_2.scheme(),
                               PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(ConditionType::kAuthority, filter_url_2.host(),
                               PatternMatchType::kLiteral, intent_filter_2);

  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);

  IntentFilters app_1_filters;
  app_1_filters.push_back(std::move(intent_filter_1));
  app_1_filters.push_back(std::move(intent_filter_2));
  IntentFilters app_2_filters;
  app_2_filters.push_back(std::move(intent_filter_3));

  FakePublisherForProxyTest pub(proxy(), AppType::kArc,
                                std::vector<std::string>{kAppId1, kAppId2});

  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(0U, GetPreferredAppsList().GetEntrySize());

  // Test that add preferred app with overlapped filters for same app will
  // add all entries.
  proxy()->SetSupportedLinksPreference(kAppId1,
                                       CloneIntentFilters(app_1_filters));

  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(2U, GetPreferredAppsList().GetEntrySize());

  // Test that add preferred app with another app that has overlapped filter
  // will clear all entries from the original app.
  proxy()->SetSupportedLinksPreference(kAppId2,
                                       CloneIntentFilters(app_2_filters));

  EXPECT_EQ(kAppId2,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(1U, GetPreferredAppsList().GetEntrySize());

  // Test that setting back to app 1 works.
  proxy()->SetSupportedLinksPreference(kAppId1,
                                       CloneIntentFilters(app_1_filters));

  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(2U, GetPreferredAppsList().GetEntrySize());
}

// Test that duplicated entry will not be added for supported links.
TEST_F(AppServiceProxyPreferredAppsTest, PreferredAppsDuplicatedSupportedLink) {
  // Test Initialize.
  GetPreferredAppsList().Init();

  const char kAppId1[] = "abcdefg";

  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");

  auto intent_filter_1 = apps_util::MakeIntentFilterForUrlScope(filter_url_1);

  auto intent_filter_2 = apps_util::MakeIntentFilterForUrlScope(filter_url_2);

  auto intent_filter_3 = apps_util::MakeIntentFilterForUrlScope(filter_url_3);

  IntentFilters app_1_filters;
  app_1_filters.push_back(std::move(intent_filter_1));
  app_1_filters.push_back(std::move(intent_filter_2));
  app_1_filters.push_back(std::move(intent_filter_3));

  FakePublisherForProxyTest pub(proxy(), AppType::kArc,
                                std::vector<std::string>{kAppId1});

  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(std::nullopt,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(0U, GetPreferredAppsList().GetEntrySize());

  proxy()->SetSupportedLinksPreference(kAppId1,
                                       CloneIntentFilters(app_1_filters));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));

  EXPECT_EQ(3U, GetPreferredAppsList().GetEntrySize());

  proxy()->SetSupportedLinksPreference(kAppId1,
                                       CloneIntentFilters(app_1_filters));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1,
            GetPreferredAppsList().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));

  EXPECT_EQ(3U, GetPreferredAppsList().GetEntrySize());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AppServiceProxyPreferredAppsTest, PreferredAppsSetSupportedLinks) {
  GetPreferredAppsList().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";
  const char kAppId3[] = "opqrstu";

  auto intent_filter_a =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.a.com/"));
  auto intent_filter_b =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.b.com/"));
  auto intent_filter_c =
      apps_util::MakeIntentFilterForUrlScope(GURL("https://www.c.com/"));

  FakeSubscriberForProxyTest sub(proxy()->profile());

  FakePublisherForProxyTest pub(
      proxy(), AppType::kArc,
      std::vector<std::string>{kAppId1, kAppId2, kAppId3});

  IntentFilters app_1_filters;
  app_1_filters.push_back(intent_filter_a->Clone());
  app_1_filters.push_back(intent_filter_b->Clone());
  proxy()->SetSupportedLinksPreference(kAppId1, std::move(app_1_filters));

  IntentFilters app_2_filters;
  app_2_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId2, std::move(app_2_filters));

  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(kAppId1, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId1, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId2, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // App 3 overlaps with both App 1 and 2. Both previous apps should have all
  // their supported link filters removed.
  IntentFilters app_3_filters;
  app_3_filters.push_back(intent_filter_b->Clone());
  app_3_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId3, std::move(app_3_filters));

  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(std::nullopt, sub.preferred_apps_list().FindPreferredAppForUrl(
                              GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId3, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId3, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // Setting App 3 as preferred again should not change anything.
  app_3_filters = std::vector<IntentFilterPtr>();
  app_3_filters.push_back(intent_filter_b->Clone());
  app_3_filters.push_back(intent_filter_c->Clone());
  proxy()->SetSupportedLinksPreference(kAppId3, std::move(app_3_filters));

  EXPECT_TRUE(pub.AppHasSupportedLinksPreference(kAppId3));
  EXPECT_EQ(kAppId3, sub.preferred_apps_list().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  proxy()->RemoveSupportedLinksPreference(kAppId3);

  EXPECT_FALSE(pub.AppHasSupportedLinksPreference(kAppId3));
  EXPECT_EQ(std::nullopt, sub.preferred_apps_list().FindPreferredAppForUrl(
                              GURL("https://www.c.com/")));
}

TEST_F(AppServiceProxyTest, LaunchCallback) {
  bool called_1 = false;
  bool called_2 = false;
  auto instance_id_1 = base::UnguessableToken::Create();
  auto instance_id_2 = base::UnguessableToken::Create();

  // If the instance is not created yet, the callback will be stored.
  {
    LaunchResult result_1;
    result_1.instance_ids.push_back(instance_id_1);
    proxy()->OnLaunched(
        base::BindOnce(
            [](bool* called, apps::LaunchResult&& launch_result) {
              *called = true;
            },
            &called_1),
        std::move(result_1));
  }
  EXPECT_EQ(proxy()->callback_list_.size(), 1U);
  EXPECT_FALSE(called_1);

  {
    LaunchResult result_2;
    result_2.instance_ids.push_back(instance_id_2);
    proxy()->OnLaunched(
        base::BindOnce(
            [](bool* called, apps::LaunchResult&& launch_result) {
              *called = true;
            },
            &called_2),
        std::move(result_2));
  }
  EXPECT_EQ(proxy()->callback_list_.size(), 2U);
  EXPECT_FALSE(called_2);

  // Once the instance is created, the callback will be called.
  {
    auto delta =
        std::make_unique<apps::Instance>("abc", instance_id_1, nullptr);
    proxy()->InstanceRegistry().OnInstance(std::move(delta));
  }
  EXPECT_EQ(proxy()->callback_list_.size(), 1U);
  EXPECT_TRUE(called_1);
  EXPECT_FALSE(called_2);

  // New callback with existing instance will be called immediately.
  called_1 = false;
  {
    LaunchResult result_3;
    proxy()->OnLaunched(
        base::BindOnce(
            [](bool* called, apps::LaunchResult&& launch_result) {
              *called = true;
            },
            &called_1),
        std::move(result_3));
  }
  EXPECT_EQ(proxy()->callback_list_.size(), 1U);
  EXPECT_TRUE(called_1);
  EXPECT_FALSE(called_2);

  // A launch that results in multiple instances.
  auto instance_id_3 = base::UnguessableToken::Create();
  auto instance_id_4 = base::UnguessableToken::Create();
  bool called_multi = false;
  {
    LaunchResult result_multi;
    result_multi.instance_ids.push_back(instance_id_3);
    result_multi.instance_ids.push_back(instance_id_4);
    proxy()->OnLaunched(
        base::BindOnce(
            [](bool* called, apps::LaunchResult&& launch_result) {
              *called = true;
            },
            &called_multi),
        std::move(result_multi));
  }
  EXPECT_EQ(proxy()->callback_list_.size(), 2U);
  EXPECT_FALSE(called_multi);
  proxy()->InstanceRegistry().OnInstance(
      std::make_unique<apps::Instance>("foo", instance_id_3, nullptr));
  proxy()->InstanceRegistry().OnInstance(
      std::make_unique<apps::Instance>("bar", instance_id_4, nullptr));
  EXPECT_EQ(proxy()->callback_list_.size(), 1U);

  EXPECT_TRUE(called_multi);
}

TEST_F(AppServiceProxyTest, GetAppsForIntentBestHandler) {
  const char kAppId1[] = "abcdefg";
  const GURL kTestUrl = GURL("https://www.example.com/");

  std::vector<AppPtr> apps;
  // A scheme-only filter that will be excluded by the |exclude_browsers|
  // parameter.
  AppPtr app = std::make_unique<App>(AppType::kWeb, kAppId1);
  app->readiness = Readiness::kReady;
  app->handles_intents = true;
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         kTestUrl.scheme(),
                                         apps::PatternMatchType::kLiteral);
  intent_filter->activity_name = "name 1";
  intent_filter->activity_label = "same label";
  app->intent_filters.push_back(std::move(intent_filter));

  // A regular mime type file filter which we expect to match.
  auto intent_filter2 = std::make_unique<apps::IntentFilter>();
  intent_filter2->AddSingleValueCondition(apps::ConditionType::kAction,
                                          apps_util::kIntentActionView,
                                          apps::PatternMatchType::kLiteral);
  intent_filter2->AddSingleValueCondition(apps::ConditionType::kFile,
                                          "text/plain",
                                          apps::PatternMatchType::kMimeType);
  intent_filter2->activity_name = "name 2";
  intent_filter2->activity_label = "same label";
  app->intent_filters.push_back(std::move(intent_filter2));

  apps.push_back(std::move(app));
  proxy()->OnApps(std::move(apps), AppType::kWeb, false);

  std::vector<apps::IntentFilePtr> files;
  auto file = std::make_unique<apps::IntentFile>(GURL("abc.txt"));
  file->mime_type = "text/plain";
  file->is_directory = false;
  files.push_back(std::move(file));
  apps::IntentPtr intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView, std::move(files));

  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      proxy()->GetAppsForIntent(intent, /*exclude_browsers=*/true);

  // Check that we actually get back the 2nd filter, and not the excluded
  // scheme-only filter which should have been discarded.
  EXPECT_EQ(1U, intent_launch_info.size());
  EXPECT_EQ("name 2", intent_launch_info[0].activity_name);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace apps
