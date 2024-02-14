// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <vector>

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class BirchCalendarProviderTest : public AshTestBase {
 public:
  BirchCalendarProviderTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchCalendar}, {});
  }
  ~BirchCalendarProviderTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BirchCalendarProviderTest, GetCalendar) {
  auto* birch_model = Shell::Get()->birch_model();

  base::RunLoop run_loop;
  birch_model->RequestBirchDataFetch(run_loop.QuitClosure());
  run_loop.Run();

  const auto& calendar_items = birch_model->GetCalendarItemsForTest();
  ASSERT_EQ(1u, calendar_items.size());
  EXPECT_EQ(u"Placeholder Event", calendar_items[0].title);
  EXPECT_FALSE(calendar_items[0].start_time.is_null());
  EXPECT_FALSE(calendar_items[0].end_time.is_null());
}

}  // namespace ash
