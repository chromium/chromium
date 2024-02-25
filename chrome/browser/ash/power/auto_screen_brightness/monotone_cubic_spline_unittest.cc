// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline.h"

#include <algorithm>
#include <random>

#include "base/check.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

TEST(MonotoneCubicSpline, Interpolation) {
  const std::vector<double> xs = {0,   10,  20,   40,   60,  80,
                                  100, 500, 1000, 2000, 3000};
  const std::vector<double> ys = {0, 5, 10, 15, 20, 25, 30, 40, 60, 80, 1000};

  const std::optional<MonotoneCubicSpline> spline =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(spline);
  EXPECT_EQ(spline->GetControlPointsY().size(), xs.size());

  // Spline's control points get their exact values.
  for (size_t i = 0; i < xs.size(); ++i) {
    EXPECT_DOUBLE_EQ(spline->Interpolate(xs[i]), ys[i]);
  }

  // Data points falling out of the range get boundary values.
  EXPECT_DOUBLE_EQ(spline->Interpolate(-0.1), ys[0]);
  EXPECT_DOUBLE_EQ(spline->Interpolate(4000), ys.back());

  // Check interpolation results on non-control points. Results are compared
  // with java implementation of Spline for Android.
  const std::vector<double> ts = {2.2, 4.8, 12.3, 46.4, 70.1, 90.5, 95.8};
  const std::vector<double> expected = {
      1.1,    2.3999999999999995, 6.200916250000001, 16.599999999999998,
      22.525, 28.08849264366124,  29.413985177197368};
  for (size_t i = 0; i < ts.size(); ++i) {
    EXPECT_DOUBLE_EQ(spline->Interpolate(ts[i]), expected[i]);
  }
}

TEST(MonotoneCubicSpline, Monotonicity) {
  const unsigned seed = 1;
  std::default_random_engine generator(seed);
  std::uniform_real_distribution<double> distribution(0.0, 200);

  std::vector<double> xs;
  std::vector<double> ys;
  for (size_t i = 0; i < 10; ++i) {
    xs.push_back(distribution(generator));
    ys.push_back(distribution(generator));
  }

  // Sort xs and ensure they are strictly increasing.
  std::sort(xs.begin(), xs.end());
  for (size_t i = 1; i < xs.size(); ++i) {
    if (xs[i] <= xs[i - 1]) {
      xs[i] = xs[i - 1] + 1;
    }
  }

  std::sort(ys.begin(), ys.end());

  const std::optional<MonotoneCubicSpline> spline =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(spline);

  std::vector<double> test_points;
  for (size_t i = 0; i < 1000; ++i) {
    test_points.push_back(distribution(generator));
  }
  std::sort(test_points.begin(), test_points.end());

  for (size_t i = 1; i < test_points.size(); ++i) {
    EXPECT_LE(spline->Interpolate(test_points[i - 1]),
              spline->Interpolate(test_points[i]));
  }
}

TEST(MonotoneCubicSpline, FromStringCorrectFormat) {
  const std::string data("1,10\n2,20\n3,30");
  const std::optional<MonotoneCubicSpline> spline_from_string =
      MonotoneCubicSpline::FromString(data);
  DCHECK(spline_from_string);
  const std::vector<double> xs = {1, 2, 3};
  const std::vector<double> ys = {10, 20, 30};
  const std::optional<MonotoneCubicSpline> expected_spline =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(expected_spline);
  EXPECT_EQ(*expected_spline, *spline_from_string);
}

TEST(MonotoneCubicSpline, FromStringTooFewRows) {
  const std::string data("1,10");
  const std::optional<MonotoneCubicSpline> spline_from_string =
      MonotoneCubicSpline::FromString(data);
  EXPECT_FALSE(spline_from_string.has_value());
}

TEST(MonotoneCubicSpline, ToString) {
  const std::vector<double> xs = {1, 2, 3};
  const std::vector<double> ys = {10, 20, 30};
  const std::optional<MonotoneCubicSpline> spline =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs, ys);
  DCHECK(spline);
  const std::string string_from_spline = spline->ToString();

  const std::string expected_string("1,10\n2,20\n3,30");

  EXPECT_EQ(expected_string, string_from_spline);
}

TEST(MonotoneCubicSpline, AssignmentAndEquality) {
  const std::vector<double> xs1 = {0,   10,  20,   40,   60,  80,
                                   100, 500, 1000, 2000, 3000};
  const std::vector<double> ys1 = {0, 5, 10, 15, 20, 25, 30, 40, 60, 80, 1000};
  std::optional<MonotoneCubicSpline> spline1 =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs1, ys1);

  const std::vector<double> xs2 = {1, 2, 3};
  const std::vector<double> ys2 = {10, 20, 30};
  const std::optional<MonotoneCubicSpline> spline2 =
      MonotoneCubicSpline::CreateMonotoneCubicSpline(xs2, ys2);

  EXPECT_NE(*spline1, *spline2);
  spline1 = spline2;

  EXPECT_EQ(*spline1, *spline2);

  const MonotoneCubicSpline spline3 = *spline1;
  EXPECT_EQ(spline3, spline2);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
