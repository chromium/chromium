// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '//resources/js/assert.js';

class PostSelectionWorklet {
  // TODO(b/335858693): Ideally, there should not be different corner lengths,
  // and instead should handle this using an inputted percent value. However,
  // this adds extra complexity to the rendering logic.
  static get inputProperties() {
    return [
      `--post-selection-corner-horizontal-length`,
      `--post-selection-corner-vertical-length`,
      `--post-selection-corner-radius`,
      `--post-selection-corner-width`,
    ];
  }

  paint(
      ctx: PaintRenderingContext2D, size: PaintSize,
      properties: StylePropertyMapReadOnly) {
    // Get inputted properties.
    const cornerLengthHorizontalProp =
        properties.get('--post-selection-corner-horizontal-length');
    const cornerLengthVerticalProp =
        properties.get('--post-selection-corner-vertical-length');
    const cornerWidthProp = properties.get('--post-selection-corner-width');
    const cornerRadiusProp = properties.get('--post-selection-corner-radius');

    // Ensure the values are in the correct format
    assertInstanceof(cornerLengthHorizontalProp, CSSUnitValue);
    assertInstanceof(cornerLengthVerticalProp, CSSUnitValue);
    assertInstanceof(cornerWidthProp, CSSUnitValue);
    assertInstanceof(cornerRadiusProp, CSSUnitValue);
    assert(
        cornerLengthHorizontalProp.unit === 'px',
        '--post-selection-corner-horizontal-length must be a pixel value');
    assert(
        cornerLengthVerticalProp.unit === 'px',
        '--post-selection-corner-vertical-length must be a pixel value');
    assert(
        cornerWidthProp.unit === 'px',
        '--post-selection-corner-width must be a pixel value');
    assert(
        cornerRadiusProp.unit === 'px',
        '--post-selection-corner-radius must be a pixel value');

    // Convert properties to integers so they are easier to use.
    const cornerLengthHorizontal = cornerLengthHorizontalProp.value;
    const cornerLengthVertical = cornerLengthVerticalProp.value;

    const cornerWidth = cornerWidthProp.value;
    // Handle cases where radius is larger than width or height
    const cornerRadius = Math.min(
        cornerRadiusProp.value,
        Math.abs(size.width / 2),
        Math.abs(size.height / 2),
    );

    const minX = cornerWidth / 2;
    const minY = cornerWidth / 2;
    // Need to subtract 1 to account for indexing by zero
    const maxX = size.width - (cornerWidth / 2) - 1;
    const maxY = size.height - (cornerWidth / 2) - 1;

    if (cornerLengthHorizontal <= 0 || cornerLengthVertical <= 0 ||
        cornerWidth <= 0) {
      return;
    }

    ctx.lineWidth = cornerWidth;
    ctx.beginPath;

    // Top-Left Corner
    ctx.moveTo(minX, cornerLengthVertical);
    ctx.arcTo(minX, minY, cornerLengthHorizontal, minY, cornerRadius);
    ctx.lineTo(cornerLengthHorizontal, minY);

    // Top-Right
    ctx.moveTo(maxX - cornerLengthHorizontal, minY);
    ctx.arcTo(maxX, minY, maxX, cornerLengthVertical, cornerRadius);
    ctx.lineTo(maxX, cornerLengthVertical);

    // Bottom-Right
    ctx.moveTo(maxX, maxY - cornerLengthVertical);
    ctx.arcTo(maxX, maxY, maxX - cornerLengthHorizontal, maxY, cornerRadius);
    ctx.lineTo(maxX - cornerLengthHorizontal, maxY);

    // Bottom-Left
    ctx.moveTo(minX + cornerLengthHorizontal, maxY);
    ctx.arcTo(minX, maxY, minX, maxY - cornerLengthVertical, cornerRadius);
    ctx.lineTo(minX, maxY - cornerLengthVertical);

    ctx.strokeStyle = 'white';
    ctx.stroke();
  }
}

registerPaint('post-selection', PostSelectionWorklet);
