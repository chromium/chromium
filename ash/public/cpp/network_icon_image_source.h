// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NETWORK_ICON_IMAGE_SOURCE_H_
#define ASH_PUBLIC_CPP_NETWORK_ICON_IMAGE_SOURCE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {
namespace network_icon {

// Number of images for signal strength arcs or bars for wireless networks.
constexpr int kNumNetworkImages = 5;

// 'NONE' will default to ARCS behavior where appropriate (e.g. no network).
enum ImageType { ARCS, BARS, NONE };

// Describes a single badge which is defined by a vector icon.
struct Badge {
  bool operator==(const Badge& other) const {
    return other.icon == icon && other.color == color;
  }
  bool operator!=(const Badge& other) const { return !(other == *this); }

  const gfx::VectorIcon* icon = nullptr;
  SkColor color;
};

// Struct to pass a collection of badges to NetworkIconImageSource.
struct Badges {
  Badge top_left = {};
  Badge center = {};
  Badge bottom_left = {};
  Badge bottom_right = {};
};

// Provides an image source for assembling a network icons.
class ASH_PUBLIC_EXPORT NetworkIconImageSource : public gfx::CanvasImageSource {
 public:
  NetworkIconImageSource(const gfx::Size& size,
                         const gfx::ImageSkia& icon,
                         const Badges& badges);
  ~NetworkIconImageSource() override;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;
  bool HasRepresentationAtAllScales() const override;

 private:
  const gfx::ImageSkia icon_;
  const Badges badges_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

// Provides an image source for wireless signal strength icons.
class ASH_PUBLIC_EXPORT SignalStrengthImageSource
    : public gfx::CanvasImageSource {
 public:
  SignalStrengthImageSource(ImageType image_type,
                            SkColor color,
                            const gfx::Size& size,
                            int signal_strength,
                            int padding = 2);

  ~SignalStrengthImageSource() override;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;
  bool HasRepresentationAtAllScales() const override;

 private:
  void DrawArcs(gfx::Canvas* canvas);
  void DrawBars(gfx::Canvas* canvas);

  ImageType image_type_;
  SkColor color_;

  // On a scale of 0 to kNumNetworkImages - 1, how connected we are.
  int signal_strength_;

  // Padding between outside of icon and edge of the canvas, in dp. This value
  // stays the same regardless of the canvas size.
  const int padding_;

  DISALLOW_COPY_AND_ASSIGN(SignalStrengthImageSource);
};

// Returns the sized full strength unbadged image for a Wi-Fi network. Used
// for wireless network notifications.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetImageForWifiNetwork(SkColor color,
                                                        gfx::Size size);

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NETWORK_ICON_IMAGE_SOURCE_H_
