// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * The left, right and top margin of the line chart, in pixels.
 * @const {number}
 */
export const CHART_MARGIN = 10;

/**
 * The default scale of the line chart. The scale means how many milliseconds
 * per pixel.
 * @const {number}
 */
export const DEFAULT_SCALE = 100;


/**
 * The minimun scale of the line chart.
 * @const {number}
 */
export const MIN_SCALE = 10;

/**
 * The maximum scale of the line chart.
 * @const {number}
 */
export const MAX_SCALE = 1000 * 60 * 3;

/**
 * The maximum number of the left and right unit label, in pixels.
 * @const {number}
 */
export const MAX_VERTICAL_LABEL_NUM = 6;

/**
 * The minimum vertical spacing between two text label of the left and right
 * unit labels, in pixels.
 * @const {number}
 */
export const MIN_LABEL_VERTICAL_SPACING = 4;

/**
 * The minimum horizontal spacing from the line chart to the left and right unit
 * labels, in pixels.
 * @const {number}
 */
export const MIN_LABEL_HORIZONTAL_SPACING = 3;

/**
 * The minimum horizontal spacing between two time tick labels, in pixels.
 * @const {number}
 */
export const MIN_TIME_LABEL_HORIZONTAL_SPACING = 25;

/**
 * The tick length of the unit tick, in pixels.
 * @const {number}
 */
export const Y_AXIS_TICK_LENGTH = 20;

/**
 * How far does the mouse wheeling to be counted as 1 unit.
 * @const {number}
 */
export const MOUSE_WHEEL_UNITS = 120;

/**
 * How far does the finger zooming to be counted as 1 unit, in pixels.
 * @const {number}
 */
export const TOUCH_ZOOM_UNITS = 60;

/**
 * The zooming rate of the line chart.
 * @const {number}
 */
export const ZOOM_RATE = 1.25;

/**
 * The mouse whell scrolling rate (For horizontal scroll), in pixels.
 * @const {number}
 */
export const MOUSE_WHEEL_SCROLL_RATE = 120;

/**
 * The drag rate, how many pixels will we move when user drag 1 pixel. Drag rate
 * is for both mouse dragging or touch dragging.
 * @const {number}
 */
export const DRAG_RATE = 3;

/**
 * The set of time step, in milliseconds. Line chart will choose a suitable one
 * from this list. The minimum and the maximum scale make sure that we can pick
 * up a unit from the list.
 * @const {Array<number>}
 */
export const TIME_STEP_UNITS = [
  1000,  // 1 second
  1000 * 5,
  1000 * 30,
  1000 * 60,  // 1 minute
  1000 * 60 * 5,
  1000 * 60 * 30,
  1000 * 60 * 60,  // 1 hour
  1000 * 60 * 60 * 5,
  1000 * 60 * 60 * 10,
];

/**
 * The sample rate of the line chart, in pixels. To reduce the cpu usage, we
 * only draw data points at the position which are exact multiple of this value.
 * @const {number}
 */
export const SAMPLE_RATE = 15;

/**
 * The text color of the line chart.
 * @const {string}
 */
export const TEXT_COLOR = '#000';

/**
 * The grid color of the line chart.
 * @const {string}
 */
export const GRID_COLOR = '#888';

/**
 * The background color of the line chart.
 * @const {string}
 */
export const BACKGROUND_COLOR = '#e3e3e3';

/**
 * The color of the menu button.
 * @const {string}
 */
export const MENU_TEXT_COLOR_LIGHT = '#e9e9e9';

/** @const{string} */
export const MENU_TEXT_COLOR_DARK = '#333';

/**
 * The enum of the align of label.
 * @enum {number}
 */
export const UnitLabelAlign = {
  LEFT: 0,
  RIGHT: 1,
};
