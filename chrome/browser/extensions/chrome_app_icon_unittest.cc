// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/macros.h"
#include "base/run_loop.h"
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
#include "ui/gfx/image/image_unittest_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/search/extension_app_result.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "components/arc/test/fake_app_instance.h"
#endif  // defined(OS_CHROMEOS)

namespace extensions {

namespace {

constexpr char kTestAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

// Receives icon image updates from ChromeAppIcon.
class TestAppIcon : public ChromeAppIconDelegate {
 public:
  TestAppIcon(content::BrowserContext* context,
              const std::string& app_id,
              int size) {
    app_icon_ =
        ChromeAppIconService::Get(context)->CreateIcon(this, app_id, size);
    DCHECK(app_icon_);
  }

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

  DISALLOW_COPY_AND_ASSIGN(TestAppIcon);
};

// Receives icon image updates from ChromeAppIconLoader.
class TestAppIconLoader : public AppIconLoaderDelegate {
 public:
  TestAppIconLoader() = default;
  ~TestAppIconLoader() override = default;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override {
    image_skia_ = image;
  }

  gfx::ImageSkia& icon() { return image_skia_; }
  const gfx::ImageSkia& icon() const { return image_skia_; }

 private:
  gfx::ImageSkia image_skia_;

  DISALLOW_COPY_AND_ASSIGN(TestAppIconLoader);
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

#if defined(OS_CHROMEOS)

bool AreAllImageRepresentationsDifferent(const gfx::ImageSkia& image1,
                                         const gfx::ImageSkia& image2) {
  const std::vector<gfx::ImageSkiaRep> image1_reps = image1.image_reps();
  const std::vector<gfx::ImageSkiaRep> image2_reps = image2.image_reps();
  DCHECK_EQ(image1_reps.size(), image2_reps.size());
  for (size_t i = 0; i < image1_reps.size(); ++i) {
    const float scale = image1_reps[i].scale();
    const gfx::ImageSkiaRep& image_rep2 = image2.GetRepresentation(scale);
    if (gfx::test::AreBitmapsClose(image1_reps[i].GetBitmap(),
                                   image_rep2.GetBitmap(), 0)) {
      return false;
    }
  }
  return true;
}

void WaitForIconUpdates(const gfx::ImageSkia& icon) {
  icon.EnsureRepsForSupportedScales();
  std::unique_ptr<gfx::ImageSkia> reference_image = icon.DeepCopy();
  while (!AreAllImageRepresentationsDifferent(*reference_image, icon))
    base::RunLoop().RunUntilIdle();
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

class ChromeAppIconTest : public ExtensionServiceTestBase {
 public:
  ChromeAppIconTest() = default;
  ~ChromeAppIconTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    const base::FilePath source_install_dir =
        data_dir().AppendASCII("app_list").AppendASCII("Extensions");
    const base::FilePath pref_path =
        source_install_dir.DirName().Append(chrome::kPreferencesFilename);
    InitializeInstalledExtensionService(pref_path, source_install_dir);
    service_->Init();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAppIconTest);
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
  EXPECT_TRUE(IsGrayscaleImage(reference_icon.image_skia()));

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

#if defined(OS_CHROMEOS)

class ChromeAppIconWithModelTest : public ChromeAppIconTest {
 public:
  ChromeAppIconWithModelTest() = default;
  ~ChromeAppIconWithModelTest() override = default;

 protected:
  void CreateBuilder() {
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<ExtensionAppModelBuilder>(controller_.get());
    builder_->Initialize(nullptr, profile(), model_updater_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  ChromeAppListItem* FindAppListItem(const std::string& app_id) {
    return model_updater_->FindItem(app_id);
  }

  test::TestAppListControllerDelegate* app_list_controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<ExtensionAppModelBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppIconWithModelTest);
};

// Validates icons sizes for various consumers.
TEST_F(ChromeAppIconWithModelTest, IconsTheSame) {
  CreateBuilder();

  // App list item is already created. Wait until all image representations are
  // updated and take image snapshot.
  ChromeAppListItem* app_list_item = FindAppListItem(kTestAppId);
  ASSERT_TRUE(app_list_item);
  WaitForIconUpdates(app_list_item->icon());
  std::unique_ptr<gfx::ImageSkia> app_list_item_image =
      app_list_item->icon().DeepCopy();

  // Load reference icon sized for the app list.
  TestAppIcon reference_icon_app_list(
      profile(), kTestAppId,
      app_list::AppListConfig::instance().grid_icon_dimension());

  // Wait until reference data is loaded.
  reference_icon_app_list.image_skia().EnsureRepsForSupportedScales();
  reference_icon_app_list.WaitForIconUpdates();
  EXPECT_FALSE(IsBlankImage(reference_icon_app_list.image_skia()));

  // Now compare with app list icon snapshot.
  EXPECT_TRUE(
      AreEqual(reference_icon_app_list.image_skia(), *app_list_item_image));

  // Load reference icon sized for the search result.
  TestAppIcon reference_icon_search(
      profile(), kTestAppId,
      app_list::AppListConfig::instance().suggestion_chip_icon_dimension());

  // Wait until reference data is loaded.
  reference_icon_search.image_skia().EnsureRepsForSupportedScales();
  reference_icon_search.WaitForIconUpdates();
  EXPECT_FALSE(IsBlankImage(reference_icon_search.image_skia()));

  app_list::ExtensionAppResult search(profile(), kTestAppId,
                                      app_list_controller(),
                                      /* is_recommendation */ true);
  WaitForIconUpdates(search.chip_icon());
  EXPECT_TRUE(AreEqual(reference_icon_search.image_skia(), search.chip_icon()));

  // Load reference icon sized for the shelf.
  TestAppIcon reference_icon_shelf(profile(), kTestAppId,
                                   extension_misc::EXTENSION_ICON_MEDIUM);

  // Wait until reference data is loaded.
  reference_icon_shelf.image_skia().EnsureRepsForSupportedScales();
  reference_icon_shelf.WaitForIconUpdates();
  EXPECT_FALSE(IsBlankImage(reference_icon_shelf.image_skia()));

  TestAppIconLoader loader_delegate;
  ChromeAppIconLoader loader(profile(), extension_misc::EXTENSION_ICON_MEDIUM,
                             &loader_delegate);
  loader.FetchImage(kTestAppId);
  WaitForIconUpdates(loader_delegate.icon());
  EXPECT_TRUE(
      AreEqual(reference_icon_shelf.image_skia(), loader_delegate.icon()));

  ResetBuilder();
}

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
  arc_test.app_instance()->RefreshAppList();
  std::vector<arc::mojom::AppInfo> fake_apps = arc_test.fake_apps();
  fake_apps[0].package_name = arc_test.fake_packages()[0].package_name;
  arc_test.app_instance()->SendRefreshAppList(fake_apps);
  arc_test.app_instance()->SendRefreshPackageList(arc_test.fake_packages());
  EXPECT_EQ(1U, reference_icon.icon_update_count());
  EXPECT_FALSE(AreEqual(reference_icon.image_skia(), image_before_badging));

  // Opts out the Play Store. Badge should be gone and icon image is the same
  // as it was before badging.
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_EQ(2U, reference_icon.icon_update_count());
  EXPECT_TRUE(AreEqual(reference_icon.image_skia(), image_before_badging));
}

#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
