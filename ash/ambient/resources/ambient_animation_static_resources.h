// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
#define ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_STATIC_RESOURCES_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/ambient_animation_theme.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Loads static resources for a given AmbientAnimationTheme. "Static" resources
// are those that are fixed for the lifetime of the animation, as opposed to
// dynamic ones that can change between animation cycles (photos from IMAX).
// All resources are only loaded one time internally, so callers are free to
// invoke these methods as many times as desired without having to worry about
// caching the output themselves.
//
// This class is not thread-safe, but it may be bound to any sequence (including
// the UI sequence). It does not do expensive blocking I/O.
class ASH_EXPORT AmbientAnimationStaticResources {
 public:
  // Creates an AmbientAnimationStaticResources instance that loads resources
  // for the given |theme|. Returns nullptr if |theme| is not supported.
  static std::unique_ptr<AmbientAnimationStaticResources> Create(
      AmbientAnimationTheme theme);

  virtual ~AmbientAnimationStaticResources() = default;

  // Returns the Lottie animation json data for this theme. The returned
  // StringPiece points to data owned by the AmbientAnimationStaticResources
  // instance. This method can never fail.
  // TODO(esum): Add an argument where the caller specifies whether to load the
  // "portrait" or "landscape" version of this animation theme.
  virtual base::StringPiece GetLottieData() const = 0;

  // Returns the image to use for a static asset in the animation, identified by
  // the |asset_id|. The |asset_id| is a string identifier specified when the
  // animation is built offline by UX and is embedded in the Lottie json file.
  // Asset ids are unique within each Lottie file, but not across Lottie files.
  //
  // Returns an empty ImageSkia instance if the |asset_id| is unknown.
  virtual gfx::ImageSkia GetStaticImageAsset(
      base::StringPiece asset_id) const = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_RESOURCES_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
