// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_

#include <optional>
#include <vector>

namespace ash {
namespace power {
namespace auto_screen_brightness {

// This class implements monotone cubic spline from Fritsch-Carlson (1980), see
// https://en.wikipedia.org/wiki/Monotone_cubic_interpolation
// This class only supports non-decreasing sequence of control points.
class MonotoneCubicSpline {
 public:
  MonotoneCubicSpline(const MonotoneCubicSpline& spline);

  MonotoneCubicSpline& operator=(const MonotoneCubicSpline& spline);

  ~MonotoneCubicSpline();

  // Parses and returns a MonotoneCubicSpline from input |data| or nullopt if
  // parsing fails. Correct formatting in |data| should be 1 row per
  // (<x>, <y>) mapping, and values of xs should strictly increase per row and
  // ys should be non-decreasing.
  static std::optional<MonotoneCubicSpline> FromString(const std::string& data);

  // Creates a MonotoneCubicSpline if inputs are valid according to the comments
  // for MonotoneCubicSpline's ctor. Otherwise returns nullopt.
  static std::optional<MonotoneCubicSpline> CreateMonotoneCubicSpline(
      const std::vector<double>& xs,
      const std::vector<double>& ys);

  bool operator==(const MonotoneCubicSpline& spline) const;
  bool operator!=(const MonotoneCubicSpline& spline) const;

  // Returns interpolated value for |x|. If |x| is smaller|greater than
  // smallest|largest value in |xs_|, then smallest|largest value in |ys_| will
  // be returned.
  double Interpolate(double x) const;

  std::vector<double> GetControlPointsX() const;

  std::vector<double> GetControlPointsY() const;

  // Converts to a string. Each (x, y) point in this curve will be converted to
  // 1 row and each (x, y) point will converted to x:y format.
  std::string ToString() const;

 private:
  // |xs| and |ys| must have the same size with at least 2 elements. |xs| must
  // be strictly increasing and |ys| must be monotone (non-decreasing).
  MonotoneCubicSpline(const std::vector<double>& xs,
                      const std::vector<double>& ys);

  std::vector<double> xs_;
  std::vector<double> ys_;

  size_t num_points_;

  // Tangents of control points.
  std::vector<double> ms_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_MONOTONE_CUBIC_SPLINE_H_
