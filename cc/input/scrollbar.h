// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_H_
#define CC_INPUT_SCROLLBAR_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/paint/paint_canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

static constexpr int kPixelsPerLineStep = 40;
static constexpr float kMinFractionToStepWhenPaging = 0.875f;

// Autoscrolling (on the main thread) happens by applying a delta every 50ms.
// Hence, pixels per second for a autoscroll cc animation can be calculated as:
// autoscroll velocity = delta / 0.05 sec = delta x 20
static constexpr float kAutoscrollMultiplier = 20.f;
static constexpr base::TimeDelta kInitialAutoscrollTimerDelay =
    base::TimeDelta::FromMilliseconds(250);

// Constants used to figure the how far out in the non-scrolling direction
// should trigger the thumb to snap back to its origin.  These calculations are
// based on observing the behavior of the MSVC8 main window scrollbar + some
// guessing/extrapolation.
static constexpr int kOffSideMultiplier = 8;
static constexpr int kDefaultWinScrollbarThickness = 17;

namespace cc {

enum ScrollbarOrientation { HORIZONTAL, VERTICAL };

enum ScrollbarPart {
  THUMB,
  TRACK_BUTTONS_TICKMARKS,  // for PartNeedsRepaint() and PaintPart() only.
  BACK_BUTTON,
  FORWARD_BUTTON,
  BACK_TRACK,
  FORWARD_TRACK,
  NO_PART,
};

class Scrollbar : public base::RefCounted<Scrollbar> {
 public:
  virtual ScrollbarOrientation Orientation() const = 0;
  virtual bool IsLeftSideVerticalScrollbar() const = 0;
  virtual bool IsSolidColor() const = 0;
  virtual bool IsOverlay() const = 0;
  virtual bool HasThumb() const = 0;
  virtual bool SupportsDragSnapBack() const = 0;

  // The following rects are all relative to the scrollbar's origin.
  // The location of ThumbRect reflects scroll offset, but cc will ignore it
  // because the compositor thread will compute thumb location from scroll
  // offset.
  virtual gfx::Rect ThumbRect() const = 0;
  virtual gfx::Rect TrackRect() const = 0;
  virtual gfx::Rect BackButtonRect() const = 0;
  virtual gfx::Rect ForwardButtonRect() const = 0;

  virtual float ThumbOpacity() const = 0;
  virtual bool HasTickmarks() const = 0;

  // Whether we need to repaint the part. Only THUMB and TRACK_BUTTONS_TICKMARKS
  // are supported.
  virtual bool NeedsRepaintPart(ScrollbarPart part) const = 0;

  // Paints the part in the given rect. The implementation should paint
  // relative to the rect, and doesn't need to know the current coordinate
  // space of |canvas|. Only THUMB, TRACK_BUTTONS_TICKMARKS are supported.
  virtual void PaintPart(PaintCanvas* canvas,
                         ScrollbarPart part,
                         const gfx::Rect& rect) = 0;

  virtual bool UsesNinePatchThumbResource() const = 0;
  virtual gfx::Size NinePatchThumbCanvasSize() const = 0;
  virtual gfx::Rect NinePatchThumbAperture() const = 0;

 protected:
  friend class base::RefCounted<Scrollbar>;
  virtual ~Scrollbar() {}
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_H_
