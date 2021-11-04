// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_constants.h"
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
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#endif

namespace apps {

class AppServiceProxyTest : public testing::Test {
 protected:
  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using AppServiceProxy = apps::AppServiceProxyChromeOs;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  using AppServiceProxy = apps::AppServiceProxyLacros;
#else
  using AppServiceProxy = apps::AppServiceProxy;
#endif

  class FakeIconLoader : public apps::IconLoader {
   public:
    void FlushPendingCallbacks() {
      for (auto& callback : pending_callbacks_) {
        auto iv = apps::mojom::IconValue::New();
        iv->icon_type = apps::mojom::IconType::kUncompressed;
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
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override {
      return apps::mojom::IconKey::New(0, 0, 0);
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
        pending_callbacks_.push_back(std::move(callback));
      }
      return nullptr;
    }

    int num_inner_finished_callbacks_ = 0;
    std::vector<apps::mojom::Publisher::LoadIconCallback> pending_callbacks_;
  };

  UniqueReleaser LoadIcon(apps::IconLoader* loader, const std::string& app_id) {
    static constexpr auto app_type = apps::mojom::AppType::kWeb;
    static constexpr auto icon_type = apps::mojom::IconType::kUncompressed;
    static constexpr int32_t size_hint_in_dip = 1;
    static bool allow_placeholder_icon = false;

    return loader->LoadIcon(app_type, app_id, icon_type, size_hint_in_dip,
                            allow_placeholder_icon,
                            base::BindOnce(&AppServiceProxyTest::OnLoadIcon,
                                           base::Unretained(this)));
  }

  void OverrideAppServiceProxyInnerIconLoader(AppServiceProxy* proxy,
                                              apps::IconLoader* icon_loader) {
    proxy->OverrideInnerIconLoaderForTesting(icon_loader);
  }

  void OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
    num_outer_finished_callbacks_++;
  }

  int NumOuterFinishedCallbacks() { return num_outer_finished_callbacks_; }

  int num_outer_finished_callbacks_ = 0;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppServiceProxyTest, IconCache) {
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

TEST_F(AppServiceProxyTest, IconCoalescer) {
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
  EXPECT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      guest_profile.get()));
  auto* guest_proxy =
      apps::AppServiceProxyFactory::GetForProfile(guest_profile.get());
  EXPECT_TRUE(guest_proxy);
  EXPECT_NE(guest_proxy, proxy);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class AppServiceProxyPreferredAppsTest : public AppServiceProxyTest {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitAndEnableFeature(
        features::kAppManagementIntentSettings);
#endif

    proxy_ = AppServiceProxyFactory::GetForProfile(&profile_);

    auto* const provider = web_app::FakeWebAppProvider::Get(&profile_);
    provider->SkipAwaitingExtensionSystem();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(&profile_);
  }

  AppServiceProxy* proxy() { return proxy_; }

  // Shortcut for adding apps to App Service without going through a real
  // Publisher.
  void OnApps(std::vector<mojom::AppPtr> apps, mojom::AppType type) {
    proxy_->OnApps(std::move(apps), type, /*should_notify_initialized=*/false);
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif

  TestingProfile profile_;
  AppServiceProxy* proxy_;
};

TEST_F(AppServiceProxyPreferredAppsTest, UpdatedOnUninstall) {
  constexpr char kTestAppId[] = "foo";
  const GURL kTestUrl = GURL("https://www.example.com/");

  // Install an app and set it as preferred for a URL.
  {
    std::vector<mojom::AppPtr> apps;
    mojom::AppPtr app = PublisherBase::MakeApp(
        mojom::AppType::kWeb, kTestAppId, mojom::Readiness::kReady, "Test App",
        mojom::InstallReason::kUser);
    app->intent_filters.push_back(
        apps_util::CreateIntentFilterForUrlScope(kTestUrl));
    apps.push_back(std::move(app));

    OnApps(std::move(apps), mojom::AppType::kWeb);
    proxy()->AddPreferredApp(kTestAppId, kTestUrl);
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
        proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(kTestAppId, preferred_app);
  }

  // Updating the app should not change its preferred app status.
  {
    std::vector<mojom::AppPtr> apps;
    mojom::AppPtr app = mojom::App::New();
    app->app_type = mojom::AppType::kWeb;
    app->app_id = kTestAppId;
    app->last_launch_time = base::Time();
    apps.push_back(std::move(app));

    OnApps(std::move(apps), mojom::AppType::kWeb);
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
        proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(kTestAppId, preferred_app);
  }

  // Uninstalling the app should remove it from the preferred app list.
  {
    std::vector<mojom::AppPtr> apps;
    mojom::AppPtr app = mojom::App::New();
    app->app_type = mojom::AppType::kWeb;
    app->app_id = kTestAppId;
    app->readiness = mojom::Readiness::kUninstalledByUser;
    apps.push_back(std::move(app));

    OnApps(std::move(apps), mojom::AppType::kWeb);
    proxy()->FlushMojoCallsForTesting();

    absl::optional<std::string> preferred_app =
        proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl);
    ASSERT_EQ(absl::nullopt, preferred_app);
  }
}

TEST_F(AppServiceProxyPreferredAppsTest, SetPreferredApp) {
  constexpr char kTestAppId1[] = "abc";
  constexpr char kTestAppId2[] = "def";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");

  auto url_filter_1 = apps_util::CreateIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::CreateIntentFilterForUrlScope(kTestUrl2);
  auto send_filter = apps_util::CreateIntentFilterForSend("image/png");

  std::vector<mojom::AppPtr> apps;
  mojom::AppPtr app1 = PublisherBase::MakeApp(
      mojom::AppType::kWeb, kTestAppId1, mojom::Readiness::kReady, "Test App",
      mojom::InstallReason::kUser);
  app1->intent_filters.push_back(url_filter_1.Clone());
  app1->intent_filters.push_back(url_filter_2.Clone());
  app1->intent_filters.push_back(send_filter.Clone());
  apps.push_back(std::move(app1));

  mojom::AppPtr app2 = PublisherBase::MakeApp(
      mojom::AppType::kWeb, kTestAppId2, mojom::Readiness::kReady, "Test App",
      mojom::InstallReason::kUser);
  app2->intent_filters.push_back(url_filter_1.Clone());
  apps.push_back(std::move(app2));

  OnApps(std::move(apps), mojom::AppType::kWeb);

  // Set app 1 as preferred. Both links should be set as preferred, but the
  // non-link filter is ignored.

  proxy()->SetSupportedLinksPreference(kTestAppId1);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(kTestAppId1,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl2));
  apps::mojom::IntentPtr mime_intent = apps::mojom::Intent::New();
  mime_intent->mime_type = "image/png";
  mime_intent->action = apps_util::kIntentActionSend;
  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredApps().FindPreferredAppForIntent(mime_intent));

  // Set app 2 as preferred. Both of the previous preferences for app 1 should
  // be removed.

  proxy()->SetSupportedLinksPreference(kTestAppId2);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId2,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl2));

  // Remove all supported link preferences for app 2.

  proxy()->RemoveSupportedLinksPreference(kTestAppId2);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl1));
}

// Using AddPreferredApp to set a supported link should enable all supported
// links for that app.
TEST_F(AppServiceProxyPreferredAppsTest, AddPreferredAppForLink) {
  constexpr char kTestAppId[] = "aaa";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");
  auto url_filter_1 = apps_util::CreateIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::CreateIntentFilterForUrlScope(kTestUrl2);

  std::vector<mojom::AppPtr> apps;
  mojom::AppPtr app1 = PublisherBase::MakeApp(
      mojom::AppType::kWeb, kTestAppId, mojom::Readiness::kReady, "Test App",
      mojom::InstallReason::kUser);
  app1->intent_filters.push_back(url_filter_1.Clone());
  app1->intent_filters.push_back(url_filter_2.Clone());
  apps.push_back(std::move(app1));
  OnApps(std::move(apps), mojom::AppType::kWeb);

  proxy()->AddPreferredApp(kTestAppId, GURL("https://www.foo.com/something/"));
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(kTestAppId,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(kTestAppId,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl2));
}

TEST_F(AppServiceProxyPreferredAppsTest, AddPreferredAppBrowser) {
  constexpr char kTestAppId1[] = "aaa";
  constexpr char kTestAppId2[] = "bbb";
  const GURL kTestUrl1 = GURL("https://www.foo.com/");
  const GURL kTestUrl2 = GURL("https://www.bar.com/");
  const GURL kTestUrl3 = GURL("https://www.baz.com/");

  auto url_filter_1 = apps_util::CreateIntentFilterForUrlScope(kTestUrl1);
  auto url_filter_2 = apps_util::CreateIntentFilterForUrlScope(kTestUrl2);
  auto url_filter_3 = apps_util::CreateIntentFilterForUrlScope(kTestUrl3);

  std::vector<mojom::AppPtr> apps;
  mojom::AppPtr app1 = PublisherBase::MakeApp(
      mojom::AppType::kWeb, kTestAppId1, mojom::Readiness::kReady, "Test App",
      mojom::InstallReason::kUser);
  app1->intent_filters.push_back(url_filter_1.Clone());
  app1->intent_filters.push_back(url_filter_2.Clone());
  apps.push_back(std::move(app1));

  mojom::AppPtr app2 = PublisherBase::MakeApp(
      mojom::AppType::kWeb, kTestAppId2, mojom::Readiness::kReady, "Test App",
      mojom::InstallReason::kUser);
  app2->intent_filters.push_back(url_filter_3.Clone());
  apps.push_back(std::move(app2));

  OnApps(std::move(apps), mojom::AppType::kWeb);

  proxy()->AddPreferredApp(kTestAppId1, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();

  // Setting "use browser" for a URL currently handled by App 1 should unset
  // both of App 1's links.
  proxy()->AddPreferredApp(apps::kUseBrowserForLink, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();

  ASSERT_EQ(apps::kUseBrowserForLink,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl1));
  ASSERT_EQ(absl::nullopt,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl2));

  proxy()->AddPreferredApp(apps::kUseBrowserForLink, kTestUrl3);
  proxy()->FlushMojoCallsForTesting();
  ASSERT_EQ(apps::kUseBrowserForLink,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl3));

  // Changing the setting back from "use browser" to App 1 should only update
  // that "use-browser" setting, settings for other URLs are unchanged.
  proxy()->AddPreferredApp(kTestAppId1, kTestUrl1);
  proxy()->FlushMojoCallsForTesting();
  ASSERT_EQ(apps::kUseBrowserForLink,
            proxy()->PreferredApps().FindPreferredAppForUrl(kTestUrl3));
}

#endif  // !BUILDFLAG(OS_CHROMEOS_LACROS)

}  // namespace apps
