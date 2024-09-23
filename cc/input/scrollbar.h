// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_H_
#define CC_INPUT_SCROLLBAR_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/paint/paint_canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

// Autoscrolling (on the main thread) happens by applying a delta every 50ms.
// Hence, pixels per second for a autoscroll cc animation can be calculated as:
// autoscroll velocity = delta / 0.05 sec = delta x 20
static constexpr float kAutoscrollMultiplier = 20.f;
static constexpr base::TimeDelta kInitialAutoscrollTimerDelay =
    base::Milliseconds(250);

// Constants used to figure the how far out in the non-scrolling direction
// should trigger the thumb to snap back to its origin.  These calculations are
// based on observing the behavior of the MSVC8 main window scrollbar + some
// guessing/extrapolation.
static constexpr int kOffSideMultiplier = 8;
static constexpr int kDefaultWinScrollbarThickness = 17;

namespace cc {

enum class ScrollbarOrientation { kHorizontal, kVertical };

enum class ScrollbarPart {
  kThumb,
  kBackButton,
  kForwardButton,
  kBackTrack,
  kForwardTrack,
  kNoPart,
};

class Scrollbar : public base::RefCounted<Scrollbar> {
 public:
  // Check if this scrollbar and the other scrollbar are backed with the same
  // source scrollbar (e.g. blink::Scrollbar).
  virtual bool IsSame(const Scrollbar&) const = 0;

  virtual ScrollbarOrientation Orientation() const = 0;
  virtual bool IsLeftSideVerticalScrollbar() const = 0;
  virtual bool IsSolidColor() const = 0;
  virtual bool IsOverlay() const = 0;
  virtual bool IsRunningWebTest() const = 0;
  virtual bool IsFluentOverlayScrollbarMinimalMode() const = 0;
  virtual bool HasThumb() const = 0;
  virtual bool SupportsDragSnapBack() const = 0;
  virtual bool JumpOnTrackClick() const = 0;
  virtual bool IsOpaque() const = 0;

  // The following rects are all relative to the scrollbar's origin.
  // The location of ThumbRect reflects scroll offset, but cc will ignore it
  // because the compositor thread will compute thumb location from scroll
  // offset.
  virtual gfx::Rect ThumbRect() const = 0;
  virtual gfx::Rect TrackRect() const = 0;
  virtual gfx::Rect BackButtonRect() const = 0;
  virtual gfx::Rect ForwardButtonRect() const = 0;

  virtual float Opacity() const = 0;
  virtual bool HasTickmarks() const = 0;

  virtual bool ThumbNeedsRepaint() const = 0;
  virtual void ClearThumbNeedsRepaint() = 0;
  virtual bool TrackAndButtonsNeedRepaint() const = 0;

  // Paints the thumb in the given rect. The implementation should paint
  // relative to the rect, and doesn't need to know the current coordinate
  // space of `canvas`. When `UsesNinePatchThumbResource()` returns true,
  // whether to use nine-patch depends on the size of `rect`. Nine-patch
  // will be disabled if the size is the full size of the thumb.
  virtual void PaintThumb(PaintCanvas& canvas, const gfx::Rect& rect) = 0;
  // Paints the track (including tickmarks if present) and buttons in the
  // given rect. See the above function about `rect`.
  virtual void PaintTrackAndButtons(PaintCanvas& canvas,
                                    const gfx::Rect& rect) = 0;

  // The thumb color for a solid color scrollbar, or a scrollbar using solid
  // color thumb. This is meaningful only when IsSolidColor() or
  // UsesSolidColorThumb() is true.
  virtual SkColor4f ThumbColor() const = 0;

  // The following two functions are called from blink only.
  // Returns true if either the track or the thumb needs repaint, or the thumb
  // moved (which doesn't need to repaint the track or the thumb in many
  // scrollbar themes).
  virtual bool NeedsUpdateDisplay() const = 0;
  virtual void ClearNeedsUpdateDisplay() = 0;

  // Whether the thumb can be painted as a nine-patch resource. Cc will stretch
  // the resource to fill the full size of the thumb. This should never change
  // during the life of the scrollbar.
  virtual bool UsesNinePatchThumbResource() const = 0;
  virtual gfx::Size NinePatchThumbCanvasSize() const = 0;
  virtual gfx::Rect NinePatchThumbAperture() const = 0;

  // Whether the thumb can be painted in a single color. This should not never
  // change during the life of the scrollbar.
  virtual bool UsesSolidColorThumb() const = 0;
  // The insets of the solid color thumb from ThumbRect().
  virtual gfx::Insets SolidColorThumbInsets() const = 0;

  // Whether the track and buttons can be painted together as a nine-patch
  // resource. Cc which will stretch the resource to fill the full size of the
  // scrollbar. This should never change during the life of the scrollbar.
  // Note that tickmarks can't be painted in nine-patch. If there are tickmarks
  // (`HasTickmarks()`), this can still return true, but the nine-patch
  // implementation in cc will not be used for painting.
  virtual bool UsesNinePatchTrackAndButtonsResource() const = 0;
  virtual gfx::Size NinePatchTrackAndButtonsCanvasSize() const = 0;
  virtual gfx::Rect NinePatchTrackAndButtonsAperture() const = 0;

  // This is used by blink only, to adjust the painted thumb rect in
  // main-thread minimal mode.
  virtual gfx::Rect ShrinkMainThreadedMinimalModeThumbRect(
      gfx::Rect& rect) const = 0;

 protected:
  friend class base::RefCounted<Scrollbar>;
  virtual ~Scrollbar() {}
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_H_
