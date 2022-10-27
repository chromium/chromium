// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const LINE_CAP = 'round';
export const LINE_WIDTH = 20;
export const MARK_RADIUS = 10;
export const MARK_COLOR = '--cros-icon-color-prominent';
export const MARK_OPACITY = '--cros-second-tone-opacity';
export const TRAIL_COLOR = '--google-blue-50';
export const TRAIL_MAX_OPACITY = 0.3;
export const SOURCE_OVER = 'source-over';
export const DESTINATION_OVER = 'destination-over';

/**
 * Get original value from css variable.
 * e.g. --cros-icon-color-prominent => rgb(232, 240, 254)
 * @param varName css variable
 * @returns original value
 */
export function lookupCssVariableValue(varName: string): string {
  return window.getComputedStyle(document.documentElement)
      .getPropertyValue(varName);
}

/**
 * Construct rgba color from rgb color and opacity value.
 * @param rgb e.g. rgb(232, 240, 254)
 * @param opacity e.g. 0.3
 * @returns rgba value. e.g. rgba(232, 240, 254, 0.3)
 */
export function constructRgba(rgb: string, opacity: string): string {
  const rgbValue = rgb.substring(rgb.indexOf('(') + 1, rgb.indexOf(')')).trim();
  return `rgba(${rgbValue}, ${opacity})`;
}

/**
 * Get trail's opacity based on touch pressure for touchscreen.
 * TODO(wenyu): this function needs further fine-tune based on the
 * distribution of pressure value. The pressure value usually is
 * around 0.3~0.6.
 */
export function getTrailOpacityFromPressure(pressure: number): string {
  return String(TRAIL_MAX_OPACITY * pressure);
}