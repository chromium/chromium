// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const char* kWebApplicationInfoTitle = "Meta Title";
const char* kDefaultManifestUrl = "https://www.example.com/manifest.json";
const char* kDefaultIconUrl = "https://www.example.com/icon.png";
const char* kDefaultManifestName = "Default Name";
const char* kDefaultManifestShortName = "Default Short Name";
const char* kDefaultStartUrl = "https://www.example.com/index.html";
const blink::mojom::DisplayMode kDefaultManifestDisplayMode =
    blink::mojom::DisplayMode::kStandalone;
const int kIconSizePx = 144;

// Tracks which of the AddToHomescreenDataFetcher::Observer methods have been
// called.
class ObserverWaiter : public AddToHomescreenDataFetcher::Observer {
 public:
  ObserverWaiter()
      : is_webapk_compatible_(false),
        title_available_(false),
        data_available_(false) {}
  ~ObserverWaiter() override {}

  // Waits till the OnDataAvailable() callback is called.
  void WaitForDataAvailable() {
    if (data_available_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnUserTitleAvailable(const base::string16& title,
                            const GURL& url,
                            bool is_webapk_compatible) override {
    // This should only be called once.
    EXPECT_FALSE(title_available_);
    EXPECT_FALSE(data_available_);
    title_available_ = true;
    title_ = title;
    is_webapk_compatible_ = is_webapk_compatible;
  }

  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       const SkBitmap& badge_icon) override {
    // This should only be called once.
    EXPECT_FALSE(data_available_);
    EXPECT_TRUE(title_available_);
    data_available_ = true;
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  base::string16 title() const { return title_; }
  bool is_webapk_compatible() const { return is_webapk_compatible_; }
  bool title_available() const { return title_available_; }

 private:
  base::string16 title_;
  bool is_webapk_compatible_;
  bool title_available_;
  bool data_available_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ObserverWaiter);
};

// Builds non-null base::NullableString16 from a UTF8 string.
base::NullableString16 NullableStringFromUTF8(const std::string& value) {
  return base::NullableString16(base::UTF8ToUTF16(value), false);
}

// Builds WebAPK compatible blink::Manifest.
blink::Manifest BuildDefaultManifest() {
  blink::Manifest manifest;
  manifest.name = NullableStringFromUTF8(kDefaultManifestName);
  manifest.short_name = NullableStringFromUTF8(kDefaultManifestShortName);
  manifest.start_url = GURL(kDefaultStartUrl);
  manifest.display = kDefaultManifestDisplayMode;

  blink::Manifest::ImageResource primary_icon;
  primary_icon.type = base::ASCIIToUTF16("image/png");
  primary_icon.sizes.push_back(gfx::Size(144, 144));
  primary_icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
  primary_icon.src = GURL(kDefaultIconUrl);
  manifest.icons.push_back(primary_icon);

  return manifest;
}

}  // anonymous namespace

class TestInstallableManager : public InstallableManager {
 public:
  explicit TestInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}

  // Mock out the GetData API so we can control exactly what is returned to the
  // data fetcher.
  void GetData(const InstallableParams& params,
               InstallableCallback callback) override {
    InstallableStatusCode code = NO_ERROR_DETECTED;
    bool is_installable = is_installable_;
    if (params.valid_primary_icon && !primary_icon_) {
      code = NO_ACCEPTABLE_ICON;
      is_installable = false;
    } else if (params.valid_manifest && params.has_worker) {
      if (!IsManifestValidForWebApp(manifest_,
                                    true /* check_webapp_manifest_display */,
                                    false /* prefer_maskable_icon */)) {
        code = valid_manifest_->errors.at(0);
        is_installable = false;
      } else if (!is_installable_) {
        code = NOT_OFFLINE_CAPABLE;
        is_installable = false;
      }
    }

    if (should_manifest_time_out_ ||
        (params.valid_manifest && params.has_worker &&
         should_installable_time_out_)) {
      return;
    }

    std::vector<InstallableStatusCode> errors;
    if (code != NO_ERROR_DETECTED)
      errors.push_back(code);
    std::move(callback).Run(
        {std::move(errors), GURL(kDefaultManifestUrl), &manifest_,
         params.valid_primary_icon ? primary_icon_url_ : GURL(),
         params.valid_primary_icon ? primary_icon_.get() : nullptr,
         params.prefer_maskable_icon,
         params.valid_badge_icon ? badge_icon_url_ : GURL(),
         params.valid_badge_icon ? badge_icon_.get() : nullptr,
         params.valid_manifest ? is_installable : false,
         params.has_worker ? is_installable : false});
  }

  void SetInstallable(bool is_installable) { is_installable_ = is_installable; }

  void SetManifest(const blink::Manifest& manifest) {
    manifest_ = manifest;

    if (!manifest.icons.empty()) {
      primary_icon_url_ = manifest_.icons[0].src;
      primary_icon_.reset(
          new SkBitmap(gfx::test::CreateBitmap(kIconSizePx, kIconSizePx)));

      badge_icon_url_ = manifest_.icons[0].src;
      badge_icon_.reset(
          new SkBitmap(gfx::test::CreateBitmap(kIconSizePx, kIconSizePx)));
    }
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

  void SetShouldInstallableTimeOut(bool should_time_out) {
    should_installable_time_out_ = should_time_out;
  }

 private:
  blink::Manifest manifest_;
  GURL primary_icon_url_;
  GURL badge_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  std::unique_ptr<SkBitmap> badge_icon_;

  bool is_installable_ = true;

  bool should_manifest_time_out_ = false;
  bool should_installable_time_out_ = false;
};

// Tests AddToHomescreenDataFetcher. These tests should be browser tests but
// Android does not support browser tests yet (crbug.com/611756).
class AddToHomescreenDataFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  AddToHomescreenDataFetcherTest() {}
  ~AddToHomescreenDataFetcherTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(profile()->CreateHistoryService(false, true));
    profile()->CreateFaviconService();

    // Manually inject the TestInstallableManager as a "InstallableManager"
    // WebContentsUserData. We can't directly call ::CreateForWebContents due to
    // typing issues since TestInstallableManager doesn't directly inherit from
    // WebContentsUserData.
    web_contents()->SetUserData(
        TestInstallableManager::UserDataKey(),
        base::WrapUnique(new TestInstallableManager(web_contents())));
    installable_manager_ = static_cast<TestInstallableManager*>(
        web_contents()->GetUserData(TestInstallableManager::UserDataKey()));

    NavigateAndCommit(GURL(kDefaultStartUrl));
  }

  std::unique_ptr<AddToHomescreenDataFetcher> BuildFetcher(
      AddToHomescreenDataFetcher::Observer* observer) {
    return std::make_unique<AddToHomescreenDataFetcher>(web_contents(), 500,
                                                        observer);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_user_title,
                  const char* expected_name,
                  blink::mojom::DisplayMode display_mode,
                  bool is_webapk_compatible) {
    WebApplicationInfo web_application_info;
    web_application_info.title = base::UTF8ToUTF16(kWebApplicationInfoTitle);

    fetcher->OnDidGetWebApplicationInfo(
        mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>(),
        web_application_info);
    waiter.WaitForDataAvailable();

    EXPECT_EQ(is_webapk_compatible, waiter.is_webapk_compatible());
    EXPECT_TRUE(waiter.title_available());
    if (is_webapk_compatible)
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_name));
    else
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_user_title));

    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().user_title,
                                  expected_user_title));
    EXPECT_EQ(display_mode, fetcher->shortcut_info().display);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_title,
                  blink::mojom::DisplayMode display_mode,
                  bool is_webapk_compatible) {
    RunFetcher(fetcher, waiter, expected_title, expected_title, display_mode,
               is_webapk_compatible);
  }

  void CheckHistograms(base::HistogramTester& histograms) {
    histograms.ExpectTotalCount("Webapp.AddToHomescreenDialog.Timeout", 1);
  }

  void SetManifest(const blink::Manifest& manifest) {
    installable_manager_->SetManifest(manifest);
  }

  void SetInstallable(bool is_installable) {
    installable_manager_->SetInstallable(is_installable);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
  }

  void SetShouldInstallableTimeOut(bool should_time_out) {
    installable_manager_->SetShouldInstallableTimeOut(should_time_out);
  }

 private:
  TestInstallableManager* installable_manager_;

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcherTest);
};

TEST_F(AddToHomescreenDataFetcherTest, EmptyManifest) {
  // Check that an empty manifest has the appropriate methods run.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false);
  CheckHistograms(histograms);
}

TEST_F(AddToHomescreenDataFetcherTest, NoIconManifest) {
  // Test a manifest with no icons. This should use the short name and have
  // a generated icon (empty icon url).
  blink::Manifest manifest = BuildDefaultManifest();
  manifest.icons.clear();
  SetManifest(manifest);

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false);
  CheckHistograms(histograms);

  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
  EXPECT_TRUE(fetcher->badge_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_badge_icon_url.is_empty());
}

// Check that the AddToHomescreenDataFetcher::Observer methods are called
// if the first call to InstallableManager::GetData() times out. This should
// fall back to the metadata title and have a non-empty icon (taken from the
// favicon).
TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check a site where InstallableManager finishes working after the time out
  // and determines PWA-ness. This is only relevant when checking WebAPK
  // compatibility.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutNonPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());
  SetInstallable(false);

  // Check where InstallableManager finishes working after the time out and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutUnknown) {
  SetShouldManifestTimeOut(true);
  SetShouldInstallableTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check where InstallableManager doesn't finish working after the time out.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false);
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

// Check that the AddToHomescreenDataFetcher::Observer methods are called if
// the service worker check times out on a page that is installable (i.e. it's
// taken too long). This should use the short_name and icon from the manifest,
// but not be WebAPK-compatible. Only relevant when checking WebAPK
// compatibility.
TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutPwa) {
  SetManifest(BuildDefaultManifest());
  SetShouldInstallableTimeOut(true);

  // Check where InstallableManager finishes working after the timeout and
  // determines PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutNonPwa) {
  SetManifest(BuildDefaultManifest());
  SetShouldInstallableTimeOut(true);
  SetInstallable(false);

  // Check where InstallableManager finishes working after the timeout and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutUnknown) {
  SetManifest(BuildDefaultManifest());
  SetShouldInstallableTimeOut(true);
  SetInstallable(false);

  // Check where InstallableManager doesn't finish working after the timeout.
  // This is akin to waiting for a service worker forever.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false);

  // Navigate to ensure the histograms are written.
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, InstallableManifest) {
  // Test a site that has an offline-capable service worker.
  blink::Manifest manifest(BuildDefaultManifest());
  SetManifest(manifest);

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             true);

  // There should always be a primary icon.
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));

  // Check that the badge icon is requested.
  EXPECT_FALSE(fetcher->badge_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_badge_icon_url,
            GURL(kDefaultIconUrl));
  CheckHistograms(histograms);
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestNameClobbersWebApplicationName) {
  // Test that when the manifest provides Manifest::name but not
  // Manifest::short_name that Manifest::name is used as the title.
  {
    // Check the case where we have no icons.
    blink::Manifest manifest = BuildDefaultManifest();
    manifest.icons.clear();
    manifest.short_name = base::NullableString16();
    SetManifest(manifest);

    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false);

    EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  blink::Manifest manifest(BuildDefaultManifest());
  manifest.short_name = base::NullableString16();
  SetManifest(manifest);

  {
    // Check a site with no offline-capable service worker.
    SetInstallable(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site where we time out waiting for the service worker.
    SetShouldInstallableTimeOut(true);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site with an offline-capable service worker.
    SetInstallable(true);
    SetShouldInstallableTimeOut(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, true);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestNoNameNoShortName) {
  // Test that when the manifest does not provide either Manifest::short_name
  // nor Manifest::name that:
  //  - The page is not WebAPK compatible.
  //  - WebApplicationInfo::title is used as the "name".
  //  - We still use the icons from the manifest.
  blink::Manifest manifest(BuildDefaultManifest());
  manifest.name = base::NullableString16();
  manifest.short_name = base::NullableString16();

  // Check the case where we don't time out waiting for the service worker.
  SetManifest(manifest);
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebApplicationInfoTitle,
             blink::mojom::DisplayMode::kStandalone, false);

  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().name,
                                kWebApplicationInfoTitle));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                kWebApplicationInfoTitle));
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}
