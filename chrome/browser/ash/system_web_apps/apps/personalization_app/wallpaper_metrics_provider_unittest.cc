// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/wallpaper_metrics_provider.h"

#include <optional>

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/hash/hash.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char kUser[] = "user1@test.com";
const AccountId kAccountId = AccountId::FromUserEmailGaiaId(kUser, kUser);

ash::personalization_app::mojom::SeaPenQueryPtr MakeTemplateQuery() {
  return ash::personalization_app::mojom::SeaPenQuery::NewTemplateQuery(
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower,
          ::base::flat_map<
              ash::personalization_app::mojom::SeaPenTemplateChip,
              ash::personalization_app::mojom::SeaPenTemplateOption>(),
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title")));
}

class WallpaperMetricsProviderTest : public ash::AshTestBase {
 public:
  WallpaperMetricsProvider& wallpaper_metrics_provider() {
    return wallpaper_metrics_provider_;
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    ash::SeaPenWallpaperManager::GetInstance()->SetSessionDelegateForTesting(
        std::make_unique<ash::TestSeaPenWallpaperManagerSessionDelegate>());
  }

 private:
  ash::InProcessDataDecoder decoder_;
  WallpaperMetricsProvider wallpaper_metrics_provider_;
};

TEST_F(WallpaperMetricsProviderTest, MissingUnitId) {
  AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  ash::WallpaperInfo info;
  info.type = ash::WallpaperType::kOnline;
  // Explicitly set unit_id to nullopt.
  info.unit_id = std::nullopt;
  info.collection_id = "test_collection_id";
  wallpaper_controller->SetUserWallpaperInfo(account_id, info);

  base::HistogramTester histogram_tester;

  wallpaper_metrics_provider().ProvideCurrentSessionData(nullptr);

  const int false_bucket = 0;
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Image.Settled.HasUnitId",
                                      false_bucket, 1);
  histogram_tester.ExpectTotalCount("Ash.Wallpaper.Image.Settled", 0);
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Collection.Settled",
                                      base::PersistentHash(info.collection_id),
                                      1);
}

TEST_F(WallpaperMetricsProviderTest, RecordsImageSettledWithUnitId) {
  AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  ash::WallpaperInfo info;
  info.type = ash::WallpaperType::kOnline;
  info.unit_id = 5u;
  info.collection_id = "test_collection_id";
  wallpaper_controller->SetUserWallpaperInfo(account_id, info);

  base::HistogramTester histogram_tester;

  wallpaper_metrics_provider().ProvideCurrentSessionData(nullptr);

  const int true_bucket = 1;
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Image.Settled.HasUnitId",
                                      true_bucket, 1);
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Image.Settled",
                                      info.unit_id.value(), 1);
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Collection.Settled",
                                      base::PersistentHash(info.collection_id),
                                      1);
}

TEST_F(WallpaperMetricsProviderTest, RecordsImageSettledWithEmptyCollectionId) {
  AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  ash::WallpaperInfo info;
  info.type = ash::WallpaperType::kOnline;
  wallpaper_controller->SetUserWallpaperInfo(account_id, info);

  base::HistogramTester histogram_tester;

  wallpaper_metrics_provider().ProvideCurrentSessionData(nullptr);

  const int false_bucket = 0;
  histogram_tester.ExpectUniqueSample("Ash.Wallpaper.Image.Settled.HasUnitId",
                                      false_bucket, 1);
  histogram_tester.ExpectTotalCount("Ash.Wallpaper.Image.Settled", 0);
  histogram_tester.ExpectUniqueSample(
      "Ash.Wallpaper.Image.Settled.HasCollectionId", false_bucket, 1);
  histogram_tester.ExpectTotalCount("Ash.Wallpaper.Collection.Settled", 0);
}

// TODO(crbug.com/347294904): Re-enable this test
TEST_F(WallpaperMetricsProviderTest, DISABLED_RecordsSeaPenTemplateSettled) {
  SimulateUserLogin(kAccountId);
  AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  gfx::ImageSkia* image = nullptr;
  std::string jpg_bytes = ash::CreateEncodedImageForTesting(
      {1, 1}, SK_ColorBLUE, data_decoder::mojom::ImageCodec::kDefault, image);
  ASSERT_TRUE(!jpg_bytes.empty());

  const uint32_t image_id = 111;
  base::test::TestFuture<bool> save_image_future;
  ash::SeaPenWallpaperManager::GetInstance()->SaveSeaPenImage(
      account_id, {std::move(jpg_bytes), image_id}, MakeTemplateQuery(),
      save_image_future.GetCallback());
  ASSERT_TRUE(save_image_future.Get());

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  ash::WallpaperInfo info(base::NumberToString(image_id),
                          ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
                          ash::WallpaperType::kSeaPen, base::Time::Now());
  wallpaper_controller->SetUserWallpaperInfo(account_id, info);

  base::HistogramTester histogram_tester;

  wallpaper_metrics_provider().ProvideCurrentSessionData(nullptr);

  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Ash.Wallpaper.SeaPen.Template.Settled",
      static_cast<int>(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower),
      1);
}

}  // namespace
