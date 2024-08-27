// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/edusumer/graduation_utils.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/edusumer/graduation_prefs.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace ash {

namespace {
constexpr int kFakeDayOfMonth = 21;
constexpr int kFakeMonth = 8;
constexpr int kFakeYear = 2024;
}  // namespace

class GraduationUtilsTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    SetFakeTimeNow();
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &GraduationUtilsTest::FakeTimeNow, nullptr, nullptr);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    graduation_prefs::RegisterProfilePrefs(pref_service_->registry());
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeTimeNow() {
    base::Time::Exploded exploded;
    exploded.day_of_month = kFakeDayOfMonth;
    exploded.month = kFakeMonth;
    exploded.year = kFakeYear;
    exploded.hour = 0;
    exploded.minute = 0;
    exploded.second = 0;
    exploded.millisecond = 0;

    EXPECT_TRUE(base::Time::FromLocalExploded(exploded, &fake_time_));
  }

 protected:
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
  static inline base::Time fake_time_;
};

TEST_F(GraduationUtilsTest, EnabledWithoutStartAndEndDates) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_TRUE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, EnabledWithPastStartDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth - 1);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_TRUE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithFutureStartDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth + 1);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithPastEndDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth - 1);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, EnabledWithFutureEndDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth + 1);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_TRUE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithPastStartAndEndDates) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth - 3);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth - 1);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithFutureStartAndEndDates) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth + 1);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth + 3);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithMissingDateFields) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth + 1);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth + 3);
  end_date.Set("month", kFakeMonth);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, EnabledWithStartAndEndDates) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth - 1);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth + 1);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_TRUE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithInvalidStartDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", 0);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, EnabledWithInvalidEndDate) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth + 1);
  end_date.Set("month", 0);
  end_date.Set("year", kFakeYear);
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, DisabledWithInvalidDateRange) {
  base::Value::Dict dict;
  dict.Set("is_enabled", true);
  base::Value::Dict start_date;
  start_date.Set("day", kFakeDayOfMonth - 1);
  start_date.Set("month", kFakeMonth);
  start_date.Set("year", kFakeYear);
  base::Value::Dict end_date;
  end_date.Set("day", kFakeDayOfMonth - 3);
  end_date.Set("month", kFakeMonth);
  end_date.Set("year", kFakeYear);
  dict.Set("start_date", start_date.Clone());
  dict.Set("end_date", end_date.Clone());
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, Disabled) {
  base::Value::Dict dict;
  dict.Set("is_enabled", false);
  pref_service()->SetManagedPref(prefs::kGraduationEnablementStatus,
                                 dict.Clone());

  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}

TEST_F(GraduationUtilsTest, EmptyPref) {
  EXPECT_FALSE(graduation::IsEligibleForGraduation(pref_service()));
}
}  // namespace ash
