// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/extensions/gfx_utils.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

constexpr char kTestAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

// Receives icon image updates from ChromeAppIcon.
class TestAppIcon : public ChromeAppIconDelegate {
 public:
  TestAppIcon(content::BrowserContext* context,
              const std::string& app_id,
              int size,
              const ChromeAppIconService::ResizeFunction& resize_function) {
    app_icon_ = ChromeAppIconService::Get(context)->CreateIcon(
        this, app_id, size, resize_function);
    DCHECK(app_icon_);
  }

  TestAppIcon(content::BrowserContext* context,
              const std::string& app_id,
              int size) {
    app_icon_ =
        ChromeAppIconService::Get(context)->CreateIcon(this, app_id, size);
    DCHECK(app_icon_);
  }

  TestAppIcon(const TestAppIcon&) = delete;
  TestAppIcon& operator=(const TestAppIcon&) = delete;

  ~TestAppIcon() override = default;

  void Reset() { app_icon_.reset(); }

  int GetIconUpdateCountAndReset() {
    int icon_update_count = icon_update_count_;
    icon_update_count_ = 0;
    return icon_update_count;
  }

  size_t icon_update_count() const { return icon_update_count_; }
  ChromeAppIcon* app_icon() { return app_icon_.get(); }
  const gfx::ImageSkia& image_skia() const { return app_icon_->image_skia(); }

  void WaitForIconUpdates() {
    base::RunLoop run_loop;
    icon_update_count_expected_ =
        icon_update_count_ + image_skia().image_reps().size();
    icon_updated_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnIconUpdated(ChromeAppIcon* icon) override {
    ++icon_update_count_;
    if (icon_update_count_ == icon_update_count_expected_ &&
        icon_updated_callback_) {
      std::move(icon_updated_callback_).Run();
    }
  }

  std::unique_ptr<ChromeAppIcon> app_icon_;

  size_t icon_update_count_ = 0;
  size_t icon_update_count_expected_ = 0;

  base::OnceClosure icon_updated_callback_;
};

// Receives icon image updates from ChromeAppIconLoader.
class TestAppIconLoader : public AppIconLoaderDelegate {
 public:
  TestAppIconLoader() = default;

  TestAppIconLoader(const TestAppIconLoader&) = delete;
  TestAppIconLoader& operator=(const TestAppIconLoader&) = delete;

  ~TestAppIconLoader() override = default;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override {
    image_skia_ = image;
  }

  gfx::ImageSkia& icon() { return image_skia_; }
  const gfx::ImageSkia& icon() const { return image_skia_; }

 private:
  gfx::ImageSkia image_skia_;
};

// Returns true if provided |image| consists from only empty pixels.
bool IsBlankImage(const gfx::ImageSkia& image) {
  if (!image.width() || !image.height())
    return false;

  const SkBitmap* bitmap = image.bitmap();
  DCHECK(bitmap);
  DCHECK_EQ(bitmap->width(), image.width());
  DCHECK_EQ(bitmap->height(), image.height());

  for (int x = 0; x < bitmap->width(); ++x) {
    for (int y = 0; y < bitmap->height(); ++y) {
      if (*bitmap->getAddr32(x, y))
        return false;
    }
  }
  return true;
}

// Returns true if provided |image| is grayscale.
bool IsGrayscaleImage(const gfx::ImageSkia& image) {
  const SkBitmap* bitmap = image.bitmap();
  DCHECK(bitmap);
  for (int x = 0; x < bitmap->width(); ++x) {
    for (int y = 0; y < bitmap->height(); ++y) {
      const unsigned int pixel = *bitmap->getAddr32(x, y);
      if ((pixel & 0xff) != ((pixel >> 8) & 0xff) ||
          (pixel & 0xff) != ((pixel >> 16) & 0xff)) {
        return false;
      }
    }
  }
  return true;
}

// Returns true if provided |image1| and |image2| are equal.
bool AreEqual(const gfx::ImageSkia& image1, const gfx::ImageSkia& image2) {
  return gfx::test::AreImagesEqual(gfx::Image(image1), gfx::Image(image2));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns true if |res| image is the |src| image with badge identified by
// |badge_type| resource. If |grayscale| is true applies HSL shift for the
// comparison.
bool IsBadgeApplied(const gfx::ImageSkia& src,
                    const gfx::ImageSkia& res,
                    ChromeAppIcon::Badge badge_type,
                    bool grayscale) {
  src.EnsureRepsForSupportedScales();
  gfx::ImageSkia reference_src = src.DeepCopy();
  if (grayscale) {
    constexpr color_utils::HSL shift = {-1, 0, 0.6};
    reference_src =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(reference_src, shift);
  }
  util::ApplyBadge(&reference_src, badge_type);

  return AreEqual(reference_src, res);
}
#endif

}  // namespace

class ChromeAppIconTest : public ExtensionServiceTestBase {
 public:
  ChromeAppIconTest() = default;

  ChromeAppIconTest(const ChromeAppIconTest&) = delete;
  ChromeAppIconTest& operator=(const ChromeAppIconTest&) = delete;

  ~ChromeAppIconTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    ASSERT_TRUE(params.ConfigureByTestDataDirectory(
        data_dir().AppendASCII("app_list")));
    InitializeExtensionService(std::move(params));
    service_->Init();
  }
};

TEST_F(ChromeAppIconTest, IconLifeCycle) {
  TestAppIcon reference_icon(profile(), kTestAppId,
                             extension_misc::EXTENSION_ICON_MEDIUM);
  EXPECT_EQ(1U, reference_icon.icon_update_count());
  // By default no representation in image.
  EXPECT_FALSE(reference_icon.image_skia().HasRepresentation(1.0f));

  // Default blank image must be provided without an update.
  EXPECT_FALSE(reference_icon.image_skia().GetRepresentation(1.0f).is_null());
  EXPECT_EQ(1U, reference_icon.icon_update_count());
  EXPECT_TRUE(reference_icon.image_skia().HasRepresentation(1.0f));
  EXPECT_EQ(extension_misc::EXTENSION_ICON_MEDIUM,
            reference_icon.image_skia().width());
  EXPECT_EQ(extension_misc::EXTENSION_ICON_MEDIUM,
            reference_icon.image_skia().height());
  EXPECT_TRUE(IsBlankImage(reference_icon.image_skia()));

  // Wait until real image is loaded.
  reference_icon.WaitForIconUpdates();
  EXPECT_EQ(2U, reference_icon.icon_update_count());
  EXPECT_FALSE(IsBlankImage(reference_icon.image_skia()));
  EXPECT_FALSE(IsGrayscaleImage(reference_icon.image_skia()));

  const gfx::ImageSkia image_before_disable = reference_icon.image_skia();
  // Disable extension. This should update icon and provide grayed image inline.
  // Note update might be sent twice in CrOS.
  service()->DisableExtension(kTestAppId, disable_reason::DISABLE_CORRUPTED);
  const size_t update_count_after_disable = reference_icon.icon_update_count();
  EXPECT_NE(2U, update_count_after_disable);
  EXPECT_FALSE(IsBlankImage(reference_icon.image_skia()));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(IsBadgeApplied(image_before_disable, reference_icon.image_skia(),
                             ChromeAppIcon::Badge::kBlocked,
                             true /* grayscale */));
#else
  EXPECT_TRUE(IsGrayscaleImage(reference_icon.image_skia()));
#endif

  // Reenable extension. It should match previous enabled image
  service()->EnableExtension(kTestAppId);
  EXPECT_NE(update_count_after_disable, reference_icon.icon_update_count());
  EXPECT_TRUE(AreEqual(reference_icon.image_skia(), image_before_disable));
}

// Validates that icon release is safe.
TEST_F(ChromeAppIconTest, IconRelease) {
  TestAppIcon test_icon1(profile(), kTestAppId,
                         extension_misc::EXTENSION_ICON_MEDIUM);
  TestAppIcon test_icon2(profile(), kTestAppId,
                         extension_misc::EXTENSION_ICON_MEDIUM);
  EXPECT_FALSE(test_icon1.image_skia().GetRepresentation(1.0f).is_null());
  EXPECT_FALSE(test_icon2.image_skia().GetRepresentation(1.0f).is_null());

  // Reset before service is stopped.
  test_icon1.Reset();

  // Reset after service is stopped.
  profile_.reset();
  test_icon2.Reset();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ChromeAppIconTest, ChromeBadging) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  TestAppIcon reference_icon(profile(), kTestAppId,
                             extension_misc::EXTENSION_ICON_MEDIUM);
  // Wait until reference data is loaded.
  EXPECT_FALSE(reference_icon.image_skia().GetRepresentation(1.0f).is_null());
  reference_icon.WaitForIconUpdates();
  EXPECT_FALSE(IsBlankImage(reference_icon.image_skia()));

  reference_icon.GetIconUpdateCountAndReset();
  const gfx::ImageSkia image_before_badging = reference_icon.image_skia();

  // Badging should be applied once package is installed.
  std::vector<arc::mojom::AppInfoPtr> fake_apps =
      ArcAppTest::CloneApps(arc_test.fake_apps());
  fake_apps[0]->package_name = arc_test.fake_packages()[0]->package_name;
  arc_test.app_instance()->SendRefreshAppList(fake_apps);
  arc_test.app_instance()->SendRefreshPackageList(
      ArcAppTest::ClonePackages(arc_test.fake_packages()));

  // Expect the package list refresh to generate two icon updates - one called
  // by ArcAppListPrefs, and one called by LaunchExtensionAppUpdate.
  EXPECT_EQ(2U, reference_icon.icon_update_count());
  EXPECT_FALSE(AreEqual(reference_icon.image_skia(), image_before_badging));
  EXPECT_TRUE(IsBadgeApplied(image_before_badging, reference_icon.image_skia(),
                             ChromeAppIcon::Badge::kChrome,
                             false /* grayscale */));

  // Opts out the Play Store. Badge should be gone and icon image is the same
  // as it was before badging.
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
  // Wait for the asynchronous ArcAppListPrefs::RemoveAllAppsAndPackages to be
  // called.
  arc_test.WaitForRemoveAllApps();
  EXPECT_TRUE(AreEqual(reference_icon.image_skia(), image_before_badging));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
