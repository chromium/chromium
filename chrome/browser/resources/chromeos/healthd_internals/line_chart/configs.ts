// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The default scale of the line chart. The scale means how many milliseconds
 * per pixel.
 */
export const DEFAULT_SCALE: number = 100;

/**
 * The minimum scale of the line chart.
 */
export const MIN_SCALE: number = 10;

/**
 * The maximum scale of the line chart.
 */
export const MAX_SCALE: number = 1000 * 60 * 3;

/**
 * How far does the mouse wheeling to be counted as 1 unit.
 */
export const MOUSE_WHEEL_UNITS: number = 120;

/**
 * How far does the finger zooming to be counted as 1 unit, in pixels.
 */
export const TOUCH_ZOOM_UNITS: number = 60;

/**
 * The zooming rate of the line chart.
 */
export const ZOOM_RATE: number = 1.25;

/**
 * The mouse wheel scrolling rate for horizontal scroll, in pixels.
 */
export const MOUSE_WHEEL_SCROLL_RATE: number = 120;

/**
 * How many pixels will we move when user drag 1 pixel. Dragging rate is for
 * both mouse dragging or touch dragging.
 */
export const DRAG_RATE: number = 3;

/**
 * The sample rate of the line chart, in pixels. To reduce the cpu usage, we
 * only draw data points at the position which are exact multiple of this value.
 */
export const SAMPLE_RATE: number = 15;

/**
 * The text color of the label in the line chart.
 */
export const TEXT_COLOR: string = '#000';

/**
 * The text size of the label in the line chart.
 */
export const TEXT_SIZE: number = 16;

/**
 * The background color of the line chart.
 */
export const BACKGROUND_COLOR: string = '#dee3e5';

/**
 * The color of the menu button.
 */
export const MENU_TEXT_COLOR_LIGHT: string = '#dee3e5';
export const MENU_TEXT_COLOR_DARK: string = '#171d1e';
