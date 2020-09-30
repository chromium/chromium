// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from './constants.js';

// Handles events specific to the PDF viewer and logs the corresponding metrics.
export class PDFMetrics {
  /**
   * Records when the zoom mode is changed to fit a FittingType.
   * @param {FittingType} fittingType the new FittingType.
   */
  static recordFitTo(fittingType) {
    if (fittingType === FittingType.FIT_TO_PAGE) {
      PDFMetrics.record(UserAction.FIT_TO_PAGE);
    } else if (fittingType === FittingType.FIT_TO_WIDTH) {
      PDFMetrics.record(UserAction.FIT_TO_WIDTH);
    }
    // There is no user action to do a fit-to-height, this only happens with
    // the open param "view=FitV".
  }

  /**
   * Records when the two up view mode is enabled or disabled.
   * @param {boolean} enabled True when two up view mode is enabled.
   */
  static recordTwoUpViewEnabled(enabled) {
    PDFMetrics.record(
        enabled ? UserAction.TWO_UP_VIEW_ENABLE :
                  UserAction.TWO_UP_VIEW_DISABLE);
  }

  /**
   * Records zoom in and zoom out actions.
   * @param {boolean} isZoomIn True when the action is zooming in, false when
   *     the action is zooming out.
   */
  static recordZoomAction(isZoomIn) {
    PDFMetrics.record(isZoomIn ? UserAction.ZOOM_IN : UserAction.ZOOM_OUT);
  }

  /**
   * Records the given action to chrome.metricsPrivate.
   * @param {UserAction} action
   */
  static record(action) {
    if (!chrome.metricsPrivate) {
      return;
    }
    if (!PDFMetrics.actionsMetric_) {
      PDFMetrics.actionsMetric_ = {
        'metricName': 'PDF.Actions',
        'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        'min': 1,
        'max': UserAction.NUMBER_OF_ACTIONS,
        'buckets': UserAction.NUMBER_OF_ACTIONS + 1
      };
    }
    chrome.metricsPrivate.recordValue(PDFMetrics.actionsMetric_, action);
    if (PDFMetrics.firstMap_.has(action)) {
      const firstAction = PDFMetrics.firstMap_.get(action);
      if (!PDFMetrics.firstActionRecorded_.has(firstAction)) {
        chrome.metricsPrivate.recordValue(
            PDFMetrics.actionsMetric_, firstAction);
        PDFMetrics.firstActionRecorded_.add(firstAction);
      }
    }
  }

  static resetForTesting() {
    PDFMetrics.firstActionRecorded_.clear();
    PDFMetrics.actionsMetric_ = null;
  }
}

/** @private {?chrome.metricsPrivate.MetricType} */
PDFMetrics.actionsMetric_ = null;

/** @private {Set} */
PDFMetrics.firstActionRecorded_ = new Set();

// Keep in sync with enums.xml.
// Do not change the numeric values or reuse them since these numbers are
// persisted to logs.
/**
 * User Actions that can be recorded by calling PDFMetrics.record.
 * The *_FIRST values are recorded automaticlly,
 * eg. PDFMetrics.record(...ROTATE) will also record ROTATE_FIRST
 * on the first instance.
 * @enum {number}
 */
export const UserAction = {
  // Recorded when the document is first loaded. This event serves as
  // denominator to determine percentages of documents in which an action was
  // taken as well as average number of each action per document.
  DOCUMENT_OPENED: 0,

  // Recorded when the document is rotated clockwise or counter-clockwise.
  ROTATE_FIRST: 1,
  ROTATE: 2,

  FIT_TO_WIDTH_FIRST: 3,
  FIT_TO_WIDTH: 4,

  FIT_TO_PAGE_FIRST: 5,
  FIT_TO_PAGE: 6,

  // Recorded when the bookmarks panel is opened.
  OPEN_BOOKMARKS_PANEL_FIRST: 7,
  OPEN_BOOKMARKS_PANEL: 8,

  // Recorded when a bookmark is followed.
  FOLLOW_BOOKMARK_FIRST: 9,
  FOLLOW_BOOKMARK: 10,

  // Recorded when the page selection is used to navigate to another page.
  PAGE_SELECTOR_NAVIGATE_FIRST: 11,
  PAGE_SELECTOR_NAVIGATE: 12,

  // Recorded when the user triggers a save of the document.
  SAVE_FIRST: 13,
  SAVE: 14,

  // Recorded when the user triggers a save of the document and the document
  // has been modified by annotations.
  SAVE_WITH_ANNOTATION_FIRST: 15,
  SAVE_WITH_ANNOTATION: 16,

  PRINT_FIRST: 17,
  PRINT: 18,

  ENTER_ANNOTATION_MODE_FIRST: 19,
  ENTER_ANNOTATION_MODE: 20,

  EXIT_ANNOTATION_MODE_FIRST: 21,
  EXIT_ANNOTATION_MODE: 22,

  // Recorded when a pen stroke is made.
  ANNOTATE_STROKE_TOOL_PEN_FIRST: 23,
  ANNOTATE_STROKE_TOOL_PEN: 24,

  // Recorded when an eraser stroke is made.
  ANNOTATE_STROKE_TOOL_ERASER_FIRST: 25,
  ANNOTATE_STROKE_TOOL_ERASER: 26,

  // Recorded when a highlighter stroke is made.
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER_FIRST: 27,
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER: 28,

  // Recorded when a stroke is made using touch.
  ANNOTATE_STROKE_DEVICE_TOUCH_FIRST: 29,
  ANNOTATE_STROKE_DEVICE_TOUCH: 30,

  // Recorded when a stroke is made using mouse.
  ANNOTATE_STROKE_DEVICE_MOUSE_FIRST: 31,
  ANNOTATE_STROKE_DEVICE_MOUSE: 32,

  // Recorded when a stroke is made using pen.
  ANNOTATE_STROKE_DEVICE_PEN_FIRST: 33,
  ANNOTATE_STROKE_DEVICE_PEN: 34,

  // Recorded when two-up view mode is enabled.
  TWO_UP_VIEW_ENABLE_FIRST: 35,
  TWO_UP_VIEW_ENABLE: 36,

  // Recorded when two-up view mode is disabled.
  TWO_UP_VIEW_DISABLE_FIRST: 37,
  TWO_UP_VIEW_DISABLE: 38,

  // Recorded when zoom in button is clicked.
  ZOOM_IN_FIRST: 39,
  ZOOM_IN: 40,

  // Recorded when zoom out button is clicked.
  ZOOM_OUT_FIRST: 41,
  ZOOM_OUT: 42,

  // Recorded when the custom zoom input field is modified.
  ZOOM_CUSTOM_FIRST: 43,
  ZOOM_CUSTOM: 44,

  // Recorded when a thumbnail is used for navigation.
  THUMBNAIL_NAVIGATE_FIRST: 45,
  THUMBNAIL_NAVIGATE: 46,

  // Recorded when the user triggers a save of the document and the document
  // has never been modified.
  SAVE_ORIGINAL_ONLY_FIRST: 47,
  SAVE_ORIGINAL_ONLY: 48,

  // Recorded when the user triggers a save of the original document, even
  // though the document has been modified.
  SAVE_ORIGINAL_FIRST: 49,
  SAVE_ORIGINAL: 50,

  // Recorded when the user triggers a save of the edited document.
  SAVE_EDITED_FIRST: 51,
  SAVE_EDITED: 52,

  // Recorded when the sidenav menu button is clicked.
  TOGGLE_SIDENAV_FIRST: 53,
  TOGGLE_SIDENAV: 54,

  // Recorded when the thumbnails button in the sidenav is clicked.
  SELECT_SIDENAV_THUMBNAILS_FIRST: 55,
  SELECT_SIDENAV_THUMBNAILS: 56,

  // Recorded when the outline button in the sidenav is clicked.
  SELECT_SIDENAV_OUTLINE_FIRST: 57,
  SELECT_SIDENAV_OUTLINE: 58,

  // Recorded when the show/hide annotations overflow menu item is clicked.
  TOGGLE_DISPLAY_ANNOTATIONS_FIRST: 59,
  TOGGLE_DISPLAY_ANNOTATIONS: 60,

  NUMBER_OF_ACTIONS: 61,
};

// Map from UserAction to the 'FIRST' action. These metrics are recorded
// by PDFMetrics.log the first time each corresponding action occurs.
/** @private Map<number, number> */
PDFMetrics.firstMap_ = new Map([
  [
    UserAction.ROTATE,
    UserAction.ROTATE_FIRST,
  ],
  [
    UserAction.FIT_TO_WIDTH,
    UserAction.FIT_TO_WIDTH_FIRST,
  ],
  [
    UserAction.FIT_TO_PAGE,
    UserAction.FIT_TO_PAGE_FIRST,
  ],
  [
    UserAction.OPEN_BOOKMARKS_PANEL,
    UserAction.OPEN_BOOKMARKS_PANEL_FIRST,
  ],
  [
    UserAction.FOLLOW_BOOKMARK,
    UserAction.FOLLOW_BOOKMARK_FIRST,
  ],
  [
    UserAction.PAGE_SELECTOR_NAVIGATE,
    UserAction.PAGE_SELECTOR_NAVIGATE_FIRST,
  ],
  [
    UserAction.SAVE,
    UserAction.SAVE_FIRST,
  ],
  [
    UserAction.SAVE_WITH_ANNOTATION,
    UserAction.SAVE_WITH_ANNOTATION_FIRST,
  ],
  [
    UserAction.PRINT,
    UserAction.PRINT_FIRST,
  ],
  [
    UserAction.ENTER_ANNOTATION_MODE,
    UserAction.ENTER_ANNOTATION_MODE_FIRST,
  ],
  [
    UserAction.EXIT_ANNOTATION_MODE,
    UserAction.EXIT_ANNOTATION_MODE_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_TOOL_PEN,
    UserAction.ANNOTATE_STROKE_TOOL_PEN_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_TOOL_ERASER,
    UserAction.ANNOTATE_STROKE_TOOL_ERASER_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER,
    UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_DEVICE_TOUCH,
    UserAction.ANNOTATE_STROKE_DEVICE_TOUCH_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_DEVICE_MOUSE,
    UserAction.ANNOTATE_STROKE_DEVICE_MOUSE_FIRST,
  ],
  [
    UserAction.ANNOTATE_STROKE_DEVICE_PEN,
    UserAction.ANNOTATE_STROKE_DEVICE_PEN_FIRST,
  ],
  [
    UserAction.TWO_UP_VIEW_ENABLE,
    UserAction.TWO_UP_VIEW_ENABLE_FIRST,
  ],
  [
    UserAction.TWO_UP_VIEW_DISABLE,
    UserAction.TWO_UP_VIEW_DISABLE_FIRST,
  ],
  [
    UserAction.ZOOM_IN,
    UserAction.ZOOM_IN_FIRST,
  ],
  [
    UserAction.ZOOM_OUT,
    UserAction.ZOOM_OUT_FIRST,
  ],
  [
    UserAction.ZOOM_CUSTOM,
    UserAction.ZOOM_CUSTOM_FIRST,
  ],
  [
    UserAction.THUMBNAIL_NAVIGATE,
    UserAction.THUMBNAIL_NAVIGATE_FIRST,
  ],
  [
    UserAction.SAVE_ORIGINAL_ONLY,
    UserAction.SAVE_ORIGINAL_ONLY_FIRST,
  ],
  [
    UserAction.SAVE_ORIGINAL,
    UserAction.SAVE_ORIGINAL_FIRST,
  ],
  [
    UserAction.SAVE_EDITED,
    UserAction.SAVE_EDITED_FIRST,
  ],
  [
    UserAction.TOGGLE_SIDENAV,
    UserAction.TOGGLE_SIDENAV_FIRST,
  ],
  [
    UserAction.SELECT_SIDENAV_THUMBNAILS,
    UserAction.SELECT_SIDENAV_THUMBNAILS_FIRST,
  ],
  [
    UserAction.SELECT_SIDENAV_OUTLINE,
    UserAction.SELECT_SIDENAV_OUTLINE_FIRST,
  ],
  [
    UserAction.TOGGLE_DISPLAY_ANNOTATIONS,
    UserAction.TOGGLE_DISPLAY_ANNOTATIONS_FIRST,
  ],
]);
