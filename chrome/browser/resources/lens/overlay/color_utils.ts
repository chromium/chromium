// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Returns an array of the Shader hex colors from the given theme.
export function getShaderLayerColorHexes(): string[] {
  return [
    '#5B5E66',
    '#8E9199',
    '#A6C8FF',
    '#EEF0F9',
    '#A8ABB3',
  ];
}

// Returns an array of the Shader rgba colors from the given theme.
export function getShaderLayerColorRgbas(): string[] {
  return [
    'rgba(91, 94, 102, 1)',
    'rgba(142, 145, 153, 1)',
    'rgba(166, 200, 255, 1)',
    'rgba(238, 240, 249, 1)',
    'rgba(168, 171, 179, 1)',
  ];
}

export interface GlifColorHexObject {
  blue: string;
  red: string;
  yellow: string;
  green: string;
}

// Object mapping the Glif colors to their corresponding hex values.
export const GLIF_HEX_COLORS: GlifColorHexObject = {
  'blue': '#3186ff',
  'red': '#ff4641',
  'yellow': '#ffd314',
  'green': '#34a853',
};

// Returns a new rgba string using the rgb values from |rgba| and the alpha
// value provided.
export function modifyRgbaTransparency(rgba: string, alpha: number): string {
  const colorsString: string = rgba.substring(5, rgba.length - 1);
  const [red, green, blue] = colorsString.split(',');
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}
