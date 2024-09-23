// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from './constants.js';

// Handles events specific to the PDF viewer and logs the corresponding metrics.

/**
 * Records when the zoom mode is changed to fit a FittingType.
 * @param fittingType the new FittingType.
 */
export function recordFitTo(fittingType: FittingType) {
  if (fittingType === FittingType.FIT_TO_PAGE) {
    record(UserAction.FIT_TO_PAGE);
  } else if (fittingType === FittingType.FIT_TO_WIDTH) {
    record(UserAction.FIT_TO_WIDTH);
  }
  // There is no user action to do a fit-to-height, this only happens with the
  // the open param "view=FitV".
}

/** Records the given action to chrome.metricsPrivate. */
export function record(action: UserAction) {
  if (!chrome.metricsPrivate) {
    return;
  }
  if (!actionsMetric) {
    actionsMetric = {
      'metricName': 'PDF.Actions',
      'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
      'min': 1,
      'max': UserAction.NUMBER_OF_ACTIONS,
      'buckets': UserAction.NUMBER_OF_ACTIONS + 1,
    };
  }
  chrome.metricsPrivate.recordValue(actionsMetric, action);
  if (firstMap.has(action)) {
    const firstAction = firstMap.get(action)!;
    if (!firstActionRecorded.has(firstAction)) {
      chrome.metricsPrivate.recordValue(actionsMetric, firstAction);
      firstActionRecorded.add(firstAction);
    }
  }
}

/** Records the given enumeration to chrome.metricsPrivate. */
export function recordEnumeration(
    enumKey: string, enumValue: number, enumSize: number) {
  if (!chrome.metricsPrivate) {
    return;
  }
  chrome.metricsPrivate.recordEnumerationValue(enumKey, enumValue, enumSize);
}

export function resetForTesting() {
  firstActionRecorded.clear();
  actionsMetric = null;
}

let actionsMetric: chrome.metricsPrivate.MetricType|null = null;
const firstActionRecorded: Set<UserAction> = new Set();

/**
 * Keep in sync with the values for enum ChromePDFViewerActions in
 * tools/metrics/histograms/metadata/pdf/enums.xml.
 * These values are persisted to logs. Entries should not be renumbered, removed
 * or reused.
 *
 * User Actions that can be recorded by calling record.
 * The *_FIRST values are recorded automaticlly,
 * eg. record(...ROTATE) will also record ROTATE_FIRST
 * on the first instance.
 */
export enum UserAction {
  // Recorded when the document is first loaded. This event serves as
  // denominator to determine percentages of documents in which an action was
  // taken as well as average number of each action per document.
  DOCUMENT_OPENED = 0,

  // Recorded when the document is rotated clockwise or counter-clockwise.
  ROTATE_FIRST = 1,
  ROTATE = 2,

  FIT_TO_WIDTH_FIRST = 3,
  FIT_TO_WIDTH = 4,

  FIT_TO_PAGE_FIRST = 5,
  FIT_TO_PAGE = 6,

  // Recorded when a bookmark is followed.
  FOLLOW_BOOKMARK_FIRST = 9,
  FOLLOW_BOOKMARK = 10,

  // Recorded when the page selection is used to navigate to another page.
  PAGE_SELECTOR_NAVIGATE_FIRST = 11,
  PAGE_SELECTOR_NAVIGATE = 12,

  // Recorded when the user triggers a save of the document.
  SAVE_FIRST = 13,
  SAVE = 14,

  // Recorded when the user triggers a save of the document and the document
  // has been modified by annotations.
  SAVE_WITH_ANNOTATION_FIRST = 15,
  SAVE_WITH_ANNOTATION = 16,

  PRINT_FIRST = 17,
  PRINT = 18,

  ENTER_ANNOTATION_MODE_FIRST = 19,
  ENTER_ANNOTATION_MODE = 20,

  EXIT_ANNOTATION_MODE_FIRST = 21,
  EXIT_ANNOTATION_MODE = 22,

  // Recorded when a pen stroke is made.
  ANNOTATE_STROKE_TOOL_PEN_FIRST = 23,
  ANNOTATE_STROKE_TOOL_PEN = 24,

  // Recorded when an eraser stroke is made.
  ANNOTATE_STROKE_TOOL_ERASER_FIRST = 25,
  ANNOTATE_STROKE_TOOL_ERASER = 26,

  // Recorded when a highlighter stroke is made.
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER_FIRST = 27,
  ANNOTATE_STROKE_TOOL_HIGHLIGHTER = 28,

  // Recorded when a stroke is made using touch.
  ANNOTATE_STROKE_DEVICE_TOUCH_FIRST = 29,
  ANNOTATE_STROKE_DEVICE_TOUCH = 30,

  // Recorded when a stroke is made using mouse.
  ANNOTATE_STROKE_DEVICE_MOUSE_FIRST = 31,
  ANNOTATE_STROKE_DEVICE_MOUSE = 32,

  // Recorded when a stroke is made using pen.
  ANNOTATE_STROKE_DEVICE_PEN_FIRST = 33,
  ANNOTATE_STROKE_DEVICE_PEN = 34,

  // Recorded when two-up view mode is enabled.
  TWO_UP_VIEW_ENABLE_FIRST = 35,
  TWO_UP_VIEW_ENABLE = 36,

  // Recorded when two-up view mode is disabled.
  TWO_UP_VIEW_DISABLE_FIRST = 37,
  TWO_UP_VIEW_DISABLE = 38,

  // Recorded when zoom in button is clicked.
  ZOOM_IN_FIRST = 39,
  ZOOM_IN = 40,

  // Recorded when zoom out button is clicked.
  ZOOM_OUT_FIRST = 41,
  ZOOM_OUT = 42,

  // Recorded when the custom zoom input field is modified.
  ZOOM_CUSTOM_FIRST = 43,
  ZOOM_CUSTOM = 44,

  // Recorded when a thumbnail is used for navigation.
  THUMBNAIL_NAVIGATE_FIRST = 45,
  THUMBNAIL_NAVIGATE = 46,

  // Recorded when the user triggers a save of the document and the document
  // has never been modified.
  SAVE_ORIGINAL_ONLY_FIRST = 47,
  SAVE_ORIGINAL_ONLY = 48,

  // Recorded when the user triggers a save of the original document, even
  // though the document has been modified.
  SAVE_ORIGINAL_FIRST = 49,
  SAVE_ORIGINAL = 50,

  // Recorded when the user triggers a save of the edited document.
  SAVE_EDITED_FIRST = 51,
  SAVE_EDITED = 52,

  // Recorded when the sidenav menu button is clicked.
  TOGGLE_SIDENAV_FIRST = 53,
  TOGGLE_SIDENAV = 54,

  // Recorded when the thumbnails button in the sidenav is clicked.
  SELECT_SIDENAV_THUMBNAILS_FIRST = 55,
  SELECT_SIDENAV_THUMBNAILS = 56,

  // Recorded when the outline button in the sidenav is clicked.
  SELECT_SIDENAV_OUTLINE_FIRST = 57,
  SELECT_SIDENAV_OUTLINE = 58,

  // Recorded when the show/hide annotations overflow menu item is clicked.
  TOGGLE_DISPLAY_ANNOTATIONS_FIRST = 59,
  TOGGLE_DISPLAY_ANNOTATIONS = 60,

  // Recorded when the present menu item is clicked.
  PRESENT_FIRST = 61,
  PRESENT = 62,

  // Recorded when the document properties menu item is clicked.
  PROPERTIES_FIRST = 63,
  PROPERTIES = 64,

  // Recorded when the attachment button in the sidenav is clicked.
  SELECT_SIDENAV_ATTACHMENT_FIRST = 65,
  SELECT_SIDENAV_ATTACHMENT = 66,

  // Recorded cut/copy/paste commands.
  CUT_FIRST = 67,
  CUT = 68,
  COPY_FIRST = 69,
  COPY = 70,
  PASTE_FIRST = 71,
  PASTE = 72,
  FIND_IN_PAGE_FIRST = 73,
  FIND_IN_PAGE = 74,

  NUMBER_OF_ACTIONS = 75,
}

function createFirstMap(): Map<UserAction, UserAction> {
  const entries = (Object.entries(UserAction) as Array<[string, number]>)
                      .filter(x => Number.isInteger(x[1]))
                      .sort((a, b) => a[1] - b[1]);

  // Exclude the first and last entries (DOCUMENT_OPENED, and NUMBER_OF_ACTIONS)
  // which don't have an equivalent "_FIRST" UserAction.
  const entriesWithFirst = entries.slice(1, entries.length - 1);
  const map = new Map();
  for (let i = 0; i < entriesWithFirst.length - 1; i += 2) {
    map.set(entriesWithFirst[i + 1]![1]!, entriesWithFirst[i]![1]!);
  }
  return map;
}

// Map from UserAction to the 'FIRST' action. These metrics are recorded
// by PDFMetrics.log the first time each corresponding action occurs.
const firstMap: Map<UserAction, UserAction> = createFirstMap();
