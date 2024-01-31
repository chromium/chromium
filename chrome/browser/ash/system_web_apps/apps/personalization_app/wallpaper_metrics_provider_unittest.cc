// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/wallpaper_metrics_provider.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/hash/hash.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

class WallpaperMetricsProviderTest : public ash::AshTestBase {
 public:
  WallpaperMetricsProvider& wallpaper_metrics_provider() {
    return wallpaper_metrics_provider_;
  }

 private:
  WallpaperMetricsProvider wallpaper_metrics_provider_;
};

TEST_F(WallpaperMetricsProviderTest, MissingUnitId) {
  AccountId account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  auto* wallpaper_controller = ash::Shell::Get()->wallpaper_controller();
  ash::WallpaperInfo info;
  info.type = ash::WallpaperType::kOnline;
  // Explicitly set unit_id to nullopt.
  info.unit_id = absl::nullopt;
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

}  // namespace
