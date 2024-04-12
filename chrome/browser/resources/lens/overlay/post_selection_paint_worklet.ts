// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

class PostSelectionWorklet {
  static get inputProperties() {
    return [
      `--corner-length`,
      `--corner-radius`,
      `--corner-width`,
    ];
  }

  paint(
      ctx: PaintRenderingContext2D, size: PaintSize,
      properties: StylePropertyMapReadOnly) {
    // Get inputted properties.
    const cornerLengthProp = properties.get('--corner-length');
    const cornerWidthProp = properties.get('--corner-width');
    const cornerRadiusProp = properties.get('--corner-radius');
    assert(cornerLengthProp);
    assert(cornerWidthProp);
    assert(cornerRadiusProp);

    // Convert properties to integers so they are easier to use.
    const cornerLength = Number.parseInt(cornerLengthProp.toString());
    const cornerWidth = Number.parseInt(cornerWidthProp.toString());
    // Handle cases where radius is larger than width or height
    const cornerRadius = Math.min(
        Number.parseInt(cornerRadiusProp.toString()),
        Math.abs(size.width / 2),
        Math.abs(size.height / 2),
    );

    const minX = cornerWidth / 2;
    const minY = cornerWidth / 2;
    // Need to subtract 1 to account for indexing by zero
    const maxX = size.width - (cornerWidth / 2) - 1;
    const maxY = size.height - (cornerWidth / 2) - 1;

    if (cornerLength <= 0 || cornerWidth <= 0) {
      return;
    }

    ctx.lineWidth = cornerWidth;
    ctx.beginPath;

    // Top-Left Corner
    ctx.moveTo(minX, cornerLength);
    ctx.arcTo(minX, minY, cornerLength, minY, cornerRadius);
    ctx.lineTo(cornerLength, minY);

    // Top-Right
    ctx.moveTo(maxX - cornerLength, minY);
    ctx.arcTo(maxX, minY, maxX, cornerLength, cornerRadius);
    ctx.lineTo(maxX, cornerLength);

    // Bottom-Right
    ctx.moveTo(maxX, maxY - cornerLength);
    ctx.arcTo(maxX, maxY, maxX - cornerLength, maxY, cornerRadius);
    ctx.lineTo(maxX - cornerLength, maxY);

    // Bottom-Left
    ctx.moveTo(minX + cornerLength, maxY);
    ctx.arcTo(minX, maxY, minX, maxY - cornerLength, cornerRadius);
    ctx.lineTo(minX, maxY - cornerLength);

    ctx.strokeStyle = 'white';
    ctx.stroke();
  }
}

registerPaint('post-selection', PostSelectionWorklet);
