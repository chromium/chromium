// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ROUNDED_CONTAINER_H_
#define ASH_STYLE_ROUNDED_CONTAINER_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ash {

// A rounded countainer which can be used in any list views to carry the items.
// It provides 4 `Behavior` styles.
class ASH_EXPORT RoundedContainer : public views::View {
  METADATA_HEADER(RoundedContainer, views::View)

 public:
  // The default empty border insets.
  static constexpr gfx::Insets kBorderInsets = gfx::Insets::VH(8, 0);

  // The default corner radius for rounded corner and non-rounded corner.
  static constexpr int kNonRoundedSideRadius = 4;
  static constexpr int kRoundedSideRadius = 16;

  enum class Behavior { kNotRounded, kTopRounded, kBottomRounded, kAllRounded };

  explicit RoundedContainer(Behavior corner_behavior = Behavior::kAllRounded,
                            int non_rounded_radius = kNonRoundedSideRadius,
                            int rounded_radius = kRoundedSideRadius);
  RoundedContainer(const RoundedContainer& other) = delete;
  RoundedContainer& operator=(const RoundedContainer& other) = delete;
  ~RoundedContainer() override;

  // Sets the corner behavior.
  void SetBehavior(Behavior behavior);

  // Sets the empty border insets.
  void SetBorderInsets(const gfx::Insets& insets);

 private:
  // Returns the corners based on the `corner_behavior_`;
  gfx::RoundedCornersF GetRoundedCorners();

  // The shape of this container. Defaults to `kAllRounded`.
  Behavior corner_behavior_;

  const float non_rounded_radius_;
  const float rounded_radius_;
};

}  // namespace ash

#endif  // ASH_STYLE_ROUNDED_CONTAINER_H_
