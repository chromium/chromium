// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Paint worklet that draws the motion overlay's blur sweep mask (see the blur
// section of motion_overlay.css). The mask is a tilted, feathered edge whose
// position follows the --indigo-sweep custom property: 0 shows none of the
// blur layer, 1 shows all of it.

// The edge is perpendicular to a 160deg CSS gradient line. The layer is opaque
// before the edge and transparent after it.
const GRADIENT_ANGLE_RADIANS: number = 160 * Math.PI / 180;
// Width of the feathered edge, as a fraction of the layer's extent along the
// gradient line.
const FEATHER_FRACTION: number = 0.25;
// Fraction of the progress spent before the edge enters the layer, and again
// after it has left it.
const PROGRESS_MARGIN: number = 0.1;

class SweepMaskWorklet {
  static get inputProperties() {
    return ['--indigo-sweep'];
  }

  paint(
      ctx: PaintRenderingContext2D, size: PaintSize,
      properties: StylePropertyMapReadOnly) {
    const value = properties.get('--indigo-sweep');
    const progress = value instanceof CSSUnitValue ? value.value : 0;
    if (progress <= 0) {
      return;
    }
    if (progress >= 1) {
      ctx.fillStyle = 'black';
      ctx.fillRect(0, 0, size.width, size.height);
      return;
    }

    // Positions along the gradient line, measured from the top-left corner.
    // The bottom-right corner is at `span`. Canvas gradients extend their end
    // colors past the gradient line, so the edge can be placed anywhere.
    const dx = Math.sin(GRADIENT_ANGLE_RADIANS);
    const dy = -Math.cos(GRADIENT_ANGLE_RADIANS);
    const span = size.width * dx + size.height * dy;
    const feather = span * FEATHER_FRACTION;
    // At PROGRESS_MARGIN the feather ends at the top-left corner, so none of
    // the layer shows. At 1 - PROGRESS_MARGIN it starts at the bottom-right
    // corner, so all of it does.
    const travel = (span + feather) / (1 - 2 * PROGRESS_MARGIN);
    const featherStart = -feather + (progress - PROGRESS_MARGIN) * travel;
    const featherEnd = featherStart + feather;

    const gradient = ctx.createLinearGradient(
        dx * featherStart, dy * featherStart, dx * featherEnd, dy * featherEnd);
    gradient.addColorStop(0, 'black');
    gradient.addColorStop(1, 'transparent');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, size.width, size.height);
  }
}

registerPaint('indigo-sweep', SweepMaskWorklet);
