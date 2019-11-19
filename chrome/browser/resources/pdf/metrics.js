// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from './pdf_fitting_type.js';

/**
 * Handles events specific to the PDF viewer and logs the corresponding metrics.
 */
export class PDFMetrics {
  /**
   * Records when the zoom mode is changed to fit a FittingType.
   *
   * @param {FittingType} fittingType the new FittingType.
   */
  static recordFitTo(fittingType) {
    if (fittingType == FittingType.FIT_TO_PAGE) {
      PDFMetrics.record(PDFMetrics.UserAction.FIT_TO_PAGE);
    } else if (fittingType == FittingType.FIT_TO_WIDTH) {
      PDFMetrics.record(PDFMetrics.UserAction.FIT_TO_WIDTH);
    }
    // There is no user action to do a fit-to-height, this only happens with
    // the open param "view=FitV".
  }

  /**
   * Records the given action to chrome.metricsPrivate.
   *
   * @param {PDFMetrics.UserAction} action
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
        'max': PDFMetrics.UserAction.NUMBER_OF_ACTIONS,
        'buckets': PDFMetrics.UserAction.NUMBER_OF_ACTIONS + 1
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
 *
 * @enum {number}
 */
PDFMetrics.UserAction = {
  /**
   * Recorded when the document is first loaded. This event serves as
   * denominator to determine percentages of documents in which an action was
   * taken as well as average number of each action per document.
   */
  DOCUMENT_OPENED: 0,

  /** Recorded when the document is rotated clockwise or counter-clockwise. */
  ROTATE_FIRST: 1,
  ROTATE: 2,

  FIT_TO_WIDTH_FIRST: 3,
  FIT_TO_WIDTH: 4,

  FIT_TO_PAGE_FIRST: 5,
  FIT_TO_PAGE: 6,

  /** Recorded when the bookmarks panel is opened. */
  OPEN_BOOKMARKS_PANEL_FIRST: 7,
  OPEN_BOOKMARKS_PANEL: 8,

  /** Recorded when a bookmark is followed. */
  FOLLOW_BOOKMARK_FIRST: 9,
  FOLLOW_BOOKMARK: 10,

  /** Recorded when the page selection is used to navigate to another page. */
  PAGE_SELECTOR_NAVIGATE_FIRST: 11,
  PAGE_SELECTOR_NAVIGATE: 12,

  /** Recorded when the user triggers a save of the document. */
  SAVE_FIRST: 13,
  SAVE: 14,

  /**
   * Recorded when the user triggers a save of the document and the document
   * has been modified by annotations.
   */
  SAVE_WITH_ANNOTATION_FIRST: 15,
  SAVE_WITH_ANNOTATION: 16,

  PRINT_FIRST: 17,
  PRINT: 18,

  ENTER_ANNOTATION_MODE_FIRST: 19,
  ENTER_ANNOTATION_MODE: 20,

  EXIT_ANNOTATION_MODE_FIRST: 21,
  EXIT_ANNOTATION_MODE: 22,

  /** Recorded when a pen stroke is made. */
  ANNOTATE_STROKE_TOOL_PEN_FIRST: 23,
  ANNOTATE_STROKE_TOOL_PEN: 24,

  /** Recorded when an eraser stroke is made. */
  ANNOTATE_STROKE_TOOL_ERASER_FIRST: 25,
  ANNOTATE_STROKE_TOOL_ERASER: 26,

  /** Recorded when a highlighter stroke is made. */
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER_FIRST: 27,
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER: 28,

  /** Recorded when a stroke is made using touch. */
  ANNOTATE_STROKE_DEVICE_TOUCH_FIRST: 29,
  ANNOTATE_STROKE_DEVICE_TOUCH: 30,

  /** Recorded when a stroke is made using mouse. */
  ANNOTATE_STROKE_DEVICE_MOUSE_FIRST: 31,
  ANNOTATE_STROKE_DEVICE_MOUSE: 32,

  /** Recorded when a stroke is made using pen. */
  ANNOTATE_STROKE_DEVICE_PEN_FIRST: 33,
  ANNOTATE_STROKE_DEVICE_PEN: 34,

  NUMBER_OF_ACTIONS: 35,
};

// Map from UserAction to the 'FIRST' action. These metrics are recorded
// by PDFMetrics.log the first time each corresponding action occurs.
/** @private Map<number, number> */
PDFMetrics.firstMap_ = new Map([
  [
    PDFMetrics.UserAction.ROTATE,
    PDFMetrics.UserAction.ROTATE_FIRST,
  ],
  [
    PDFMetrics.UserAction.FIT_TO_WIDTH,
    PDFMetrics.UserAction.FIT_TO_WIDTH_FIRST,
  ],
  [
    PDFMetrics.UserAction.FIT_TO_PAGE,
    PDFMetrics.UserAction.FIT_TO_PAGE_FIRST,
  ],
  [
    PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL,
    PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL_FIRST,
  ],
  [
    PDFMetrics.UserAction.FOLLOW_BOOKMARK,
    PDFMetrics.UserAction.FOLLOW_BOOKMARK_FIRST,
  ],
  [
    PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE,
    PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE_FIRST,
  ],
  [
    PDFMetrics.UserAction.SAVE,
    PDFMetrics.UserAction.SAVE_FIRST,
  ],
  [
    PDFMetrics.UserAction.SAVE_WITH_ANNOTATION,
    PDFMetrics.UserAction.SAVE_WITH_ANNOTATION_FIRST,
  ],
  [
    PDFMetrics.UserAction.PRINT,
    PDFMetrics.UserAction.PRINT_FIRST,
  ],
  [
    PDFMetrics.UserAction.ENTER_ANNOTATION_MODE,
    PDFMetrics.UserAction.ENTER_ANNOTATION_MODE_FIRST,
  ],
  [
    PDFMetrics.UserAction.EXIT_ANNOTATION_MODE,
    PDFMetrics.UserAction.EXIT_ANNOTATION_MODE_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_PEN,
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_PEN_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_ERASER,
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_ERASER_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER,
    PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_TOUCH,
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_TOUCH_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_MOUSE,
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_MOUSE_FIRST,
  ],
  [
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_PEN,
    PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_PEN_FIRST,
  ],
]);
