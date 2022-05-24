// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_features.h"
#endif

// TODO(crbug.com/1253250):  Remove mojom related test cases.

namespace apps {

class AppServiceProxyTest : public testing::Test {
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
        AppType app_type,
        const std::string& app_id,
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

    std::unique_ptr<Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override {
      if (icon_type == apps::mojom::IconType::kUncompressed) {
        pending_callbacks_.push_back(
            IconValueToMojomIconValueCallback(std::move(callback)));
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

  int NumOuterFinishedCallbacks() { return num_outer_finished_callbacks_; }

  int num_outer_finished_callbacks_ = 0;

  content::BrowserTaskEnvironment task_environment_;
};

class AppServiceProxyIconTest : public AppServiceProxyTest,
                                public ::testing::WithParamInterface<bool> {
 protected:
  bool IsLoadIconWithoutMojomEnabled() const { return GetParam(); }

  UniqueReleaser LoadIcon(apps::IconLoader* loader, const std::string& app_id) {
    static constexpr int32_t size_hint_in_dip = 1;
    static bool allow_placeholder_icon = false;

    if (IsLoadIconWithoutMojomEnabled()) {
      static constexpr auto app_type = AppType::kWeb;
      static constexpr auto icon_type = IconType::kUncompressed;
      return loader->LoadIcon(
          app_type, app_id, icon_type, size_hint_in_dip, allow_placeholder_icon,
          base::BindOnce([](int* num_callbacks,
                            apps::IconValuePtr icon) { ++(*num_callbacks); },
                         &num_outer_finished_callbacks_));
    } else {
      static constexpr auto app_type = apps::mojom::AppType::kWeb;
      static constexpr auto icon_type = apps::mojom::IconType::kUncompressed;
      return loader->LoadIcon(
          app_type, app_id, icon_type, size_hint_in_dip, allow_placeholder_icon,
          base::BindOnce(
              [](int* num_callbacks, apps::mojom::IconValuePtr icon) {
                ++(*num_callbacks);
              },
              &num_outer_finished_callbacks_));
    }
  }
};

TEST_P(AppServiceProxyIconTest, IconCache) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCache code, see icon_cache_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCache but also other IconLoader filters, such as an IconCoalescer.

  AppServiceProxy proxy(nullptr);
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(&proxy, &fake);

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c0 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(1, NumOuterFinishedCallbacks());

  // The next LoadIcon call should be a cache hit.
  UniqueReleaser c1 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // Destroy the IconLoader::Releaser's, clearing the cache.
  c0.reset();
  c1.reset();

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c2 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(3, NumOuterFinishedCallbacks());
}

TEST_P(AppServiceProxyIconTest, IconCoalescer) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCoalescer code, see icon_coalescer_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCoalescer but also other IconLoader filters, such as an IconCache.

  AppServiceProxy proxy(nullptr);

  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(&proxy, &fake);

  // Issue 4 LoadIcon requests, 2 after de-duplication.
  UniqueReleaser a0 = LoadIcon(&proxy, "avocet");
  UniqueReleaser a1 = LoadIcon(&proxy, "avocet");
  UniqueReleaser b2 = LoadIcon(&proxy, "brolga");
  UniqueReleaser a3 = LoadIcon(&proxy, "avocet");
  EXPECT_EQ(2, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // Resolve their responses.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Issue another request, that triggers neither IconCache nor IconCoalescer.
  UniqueReleaser c4 = LoadIcon(&proxy, "curlew");
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
  UniqueReleaser c5 = LoadIcon(&proxy, "curlew");
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
  // TODO(https://crbug.com/1122463): this should be nullptr once we address all
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

// The parameter indicates whether the kAppServiceLoadIconWithoutMojom feature
// is enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         AppServiceProxyIconTest,
                         ::testing::Values(true, false));

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class AppServiceProxyPreferredAppsTest : public AppServiceProxyTest {
 public:
  void SetUp() override {
    proxy_ = AppServiceProxyFactory::GetForProfile(&profile_);

    web_app::test::AwaitStartWebAppProviderAndSubsystems(&profile_);
  }

  AppServiceProxy* proxy() { return proxy_; }

  // Shortcut for adding apps to App Service without going through a real
  // Publisher.
  void OnApps(std::vector<AppPtr> apps, AppType type) {
    if (base::FeatureList::IsEnabled(kAppServiceOnAppUpdateWithoutMojom)) {
      proxy_->OnApps(std::move(apps), type,
                     /*should_notify_initialized=*/false);
    } else {
      std::vector<mojom::AppPtr> mojom_apps;
      for (const auto& app : apps) {
        mojom_apps.push_back(ConvertAppToMojomApp(app));
      }
      proxy_->OnApps(std::move(mojom_apps), ConvertAppTypeToMojomAppType(type),
                     /*should_notify_initialized=*/false);
    }
  }

 private:
  TestingProfile profile_;
  raw_ptr<AppServiceProxy> proxy_;
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
    proxy()->AddPreferredApp(kTestAppId, kTestUrl);
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
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
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
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
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
        proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(absl::nullopt, preferred_app);
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
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));
  auto mime_intent = std::make_unique<Intent>(apps_util::kIntentActionSend);
  mime_intent->mime_type = "image/png";
  ASSERT_EQ(
      absl::nullopt,
      proxy()->PreferredAppsList().FindPreferredAppForIntent(mime_intent));

  // Set app 2 as preferred. Both of the previous preferences for app 1 should
  // be removed.

  proxy()->SetSupportedLinksPreference(kTestAppId2);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId2,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));

  // Remove all supported link preferences for app 2.

  proxy()->RemoveSupportedLinksPreference(kTestAppId2);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
}

// Using AddPreferredApp to set a supported link should enable all supported
// links for that app.
TEST_F(AppServiceProxyPreferredAppsTest, AddPreferredAppForLink) {
  constexpr char kTestAppId[] = "aaa";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");
  auto url_filter_1 = apps_util::MakeIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::MakeIntentFilterForUrlScope(kTestUrl2);

  std::vector<AppPtr> apps;
  AppPtr app1 = std::make_unique<App>(AppType::kWeb, kTestAppId);
  app1->readiness = Readiness::kReady;
  app1->intent_filters.push_back(url_filter_1->Clone());
  app1->intent_filters.push_back(url_filter_2->Clone());
  apps.push_back(std::move(app1));
  OnApps(std::move(apps), AppType::kWeb);

  proxy()->AddPreferredApp(kTestAppId, GURL("https://www.foo.com/something/"));
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(kTestAppId,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));
}

TEST_F(AppServiceProxyPreferredAppsTest, AddPreferredAppBrowser) {
  constexpr char kTestAppId1[] = "aaa";
  constexpr char kTestAppId2[] = "bbb";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");
  const GURL kTestUrl3 = GURL("https://www.baz.com/");

  auto url_filter_1 = apps_util::MakeIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::MakeIntentFilterForUrlScope(kTestUrl2);
  auto url_filter_3 = apps_util::MakeIntentFilterForUrlScope(kTestUrl3);

  std::vector<AppPtr> apps;
  AppPtr app1 = std::make_unique<App>(AppType::kWeb, kTestAppId1);
  app1->readiness = Readiness::kReady;
  app1->intent_filters.push_back(url_filter_1->Clone());
  app1->intent_filters.push_back(url_filter_2->Clone());
  apps.push_back(std::move(app1));

  AppPtr app2 = std::make_unique<App>(AppType::kWeb, kTestAppId2);
  app2->readiness = Readiness::kReady;
  app2->intent_filters.push_back(url_filter_3->Clone());
  apps.push_back(std::move(app2));

  OnApps(std::move(apps), AppType::kWeb);

  proxy()->AddPreferredApp(kTestAppId1, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();

  // Setting "use browser" for a URL currently handled by App 1 should unset
  // both of App 1's links.
  proxy()->AddPreferredApp(apps_util::kUseBrowserForLink, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(apps_util::kUseBrowserForLink,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl2));

  proxy()->AddPreferredApp(apps_util::kUseBrowserForLink, kTestUrl3);
  proxy()->FlushMojoCallsForTesting();
  ASSERT_EQ(apps_util::kUseBrowserForLink,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl3));

  // Changing the setting back from "use browser" to App 1 should only update
  // that "use-browser" setting, settings for other URLs are unchanged.
  proxy()->AddPreferredApp(kTestAppId1, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();
  ASSERT_EQ(apps_util::kUseBrowserForLink,
            proxy()->PreferredAppsList().FindPreferredAppForUrl(kTestUrl3));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AppServiceProxyTest, LaunchCallback) {
  AppServiceProxy proxy(nullptr);
  bool called_1 = false;
  bool called_2 = false;
  auto instance_id_1 = base::UnguessableToken::Create();
  LaunchResult result_1;
  result_1.instance_ids.push_back(instance_id_1);
  auto instance_id_2 = base::UnguessableToken::Create();
  LaunchResult result_2;
  result_2.instance_ids.push_back(instance_id_2);

  // If the instance is not created yet, the callback will be stored.
  proxy.OnLaunched(
      base::BindOnce([](bool* called,
                        apps::LaunchResult&& launch_result) { *called = true; },
                     &called_1),
      std::move(result_1));
  EXPECT_EQ(proxy.callback_list_.size(), 1U);
  EXPECT_FALSE(called_1);

  proxy.OnLaunched(
      base::BindOnce([](bool* called,
                        apps::LaunchResult&& launch_result) { *called = true; },
                     &called_2),
      std::move(result_2));
  EXPECT_EQ(proxy.callback_list_.size(), 2U);
  EXPECT_FALSE(called_2);

  // Once the instance is created, the callback will be called.
  auto delta = std::make_unique<apps::Instance>("abc", instance_id_1, nullptr);
  proxy.InstanceRegistry().OnInstance(std::move(delta));
  EXPECT_EQ(proxy.callback_list_.size(), 1U);
  EXPECT_TRUE(called_1);
  EXPECT_FALSE(called_2);

  // New callback with existing instance will be called immediately.
  called_1 = false;
  proxy.OnLaunched(
      base::BindOnce([](bool* called,
                        apps::LaunchResult&& launch_result) { *called = true; },
                     &called_1),
      std::move(result_1));
  EXPECT_EQ(proxy.callback_list_.size(), 1U);
  EXPECT_TRUE(called_1);
  EXPECT_FALSE(called_2);

  // A launch that results in multiple instances.
  LaunchResult result_multi;
  auto instance_id_3 = base::UnguessableToken::Create();
  auto instance_id_4 = base::UnguessableToken::Create();
  result_multi.instance_ids.push_back(instance_id_3);
  result_multi.instance_ids.push_back(instance_id_4);
  bool called_multi = false;
  proxy.OnLaunched(
      base::BindOnce([](bool* called,
                        apps::LaunchResult&& launch_result) { *called = true; },
                     &called_multi),
      std::move(result_multi));
  EXPECT_EQ(proxy.callback_list_.size(), 2U);
  EXPECT_FALSE(called_multi);
  proxy.InstanceRegistry().OnInstance(
      std::make_unique<apps::Instance>("foo", instance_id_3, nullptr));
  proxy.InstanceRegistry().OnInstance(
      std::make_unique<apps::Instance>("bar", instance_id_4, nullptr));
  EXPECT_EQ(proxy.callback_list_.size(), 1U);

  EXPECT_TRUE(called_multi);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace apps
