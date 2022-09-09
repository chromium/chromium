// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   show: boolean,
 *   anchor: (!chrome.accessibilityPrivate.ScreenRect|undefined),
 *   isPaused: (boolean|undefined),
 *   speed: (number|undefined),
 * }}
 */
let SelectToSpeakPanelState;

/*
 * A mock AccessibilityPrivate API for tests.
 */
var MockAccessibilityPrivate = {
  FocusType: {
    SOLID: 'solid',
  },

  AccessibilityFeature: {DICTATION_PUMPKIN_PARSING: 'dictationPumpkinParsing'},

  DictationBubbleIconType: {
    HIDDEN: 'hidden',
    STANDBY: 'standby',
    MACRO_SUCCESS: 'macroSuccess',
    MACRO_FAIL: 'macroFail',
  },

  DictationBubbleHintType: {
    TRY_SAYING: 'trySaying',
    TYPE: 'type',
    DELETE: 'delete',
    SELECT_ALL: 'selectAll',
    UNDO: 'undo',
    HELP: 'help',
    UNSELECT: 'unselect',
    COPY: 'copy',
  },

  SyntheticKeyboardEventType: {KEYDOWN: 'keydown', KEYUP: 'keyup,'},

  /** @private {function<number, number>} */
  boundsListener_: null,

  /**
   * @private {function(!chrome.accessibilityPrivate.SelectToSpeakPanelAction,
   *     number=)}
   */
  selectToSpeakPanelActionListener_: null,

  /** @private {function()} */
  selectToSpeakStateChangeListener_: null,

  /** @private {!chrome.accessibilityPrivate.ScreenRect} */
  scrollableBounds_: {},

  /** @private {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} */
  focusRings_: [],
  handleScrollableBoundsForPointFoundCallback_: null,
  moveMagnifierToRectCallback_: null,

  /** @private {?SelectToSpeakPanelState} */
  selectToSpeakPanelState_: null,

  /** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  highlightRects_: [],

  /** @private {?string} */
  highlightColor_: null,

  /** @private {function<boolean>} */
  dictationToggleListener_: null,

  /** @private {boolean} */
  dictationActivated_: false,

  /** @private {!chrome.accessibilityPrivate.DictationBubbleProperties|null} */
  dictationBubbleProps_: null,

  /** @private {Set<string>} */
  enabledFeatures_: new Set(),

  /** @private {number} */
  spokenFeedbackSilenceCount_: 0,

  // Methods from AccessibilityPrivate API. //

  onScrollableBoundsForPointRequested: {
    /**
     * Adds a listener to onScrollableBoundsForPointRequested.
     * @param {function<number, number>} listener
     */
    addListener: listener => {
      MockAccessibilityPrivate.boundsListener_ = listener;
    },

    /**
     * Removes the listener.
     * @param {function<number, number>} listener
     */
    removeListener: listener => {
      if (MockAccessibilityPrivate.boundsListener_ === listener) {
        MockAccessibilityPrivate.boundsListener_ = null;
      }
    },
  },

  onMagnifierBoundsChanged:
      {addListener: listener => {}, removeListener: listener => {}},

  onSelectToSpeakPanelAction: {
    /**
     * Adds a listener to onSelectToSpeakPanelAction.
     * @param {!function(!chrome.accessibilityPrivate.SelectToSpeakPanelAction,
     *     number=)} listener
     */
    addListener: listener => {
      MockAccessibilityPrivate.selectToSpeakPanelActionListener_ = listener;
    },
  },

  onToggleDictation: {
    /**
     * Adds a listener to onToggleDictation.
     * @param {function<boolean>} listener
     */
    addListener: listener => {
      MockAccessibilityPrivate.dictationToggleListener_ = listener;
    },

    /**
     * Removes the listener.
     * @param {function<boolean>} listener
     */
    removeListener: listener => {
      if (MockAccessibilityPrivate.dictationToggleListener_ === listener) {
        MockAccessibilityPrivate.dictationToggleListener_ = null;
      }
    },
  },

  onSelectToSpeakStateChangeRequested: {
    /**
     * Adds a listener to onSelectToSpeakStateChangeRequested.
     * @param {!function()} listener
     */
    addListener: listener => {
      MockAccessibilityPrivate.selectToSpeakStateChangeListener_ = listener;
    },
  },

  /**
   * Called when AccessibilityCommon wants to enable mouse events.
   * @param {boolean} enabled
   */
  enableMouseEvents: enabled => {},

  /**
   * Called when AccessibilityCommon finds scrollable bounds at a point.
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   */
  handleScrollableBoundsForPointFound: bounds => {
    MockAccessibilityPrivate.scrollableBounds_ = bounds;
    MockAccessibilityPrivate.handleScrollableBoundsForPointFoundCallback_();
  },

  /**
   * Called when AccessibilityCommon wants to move the magnifier viewport to
   * include a specific rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   */
  moveMagnifierToRect: rect => {
    if (MockAccessibilityPrivate.moveMagnifierToRectCallback_) {
      MockAccessibilityPrivate.moveMagnifierToRectCallback_(rect);
    }
  },

  /**
   * Called when AccessibilityCommon wants to set the focus rings. We can
   * assume that it is only setting one set of rings at a time, and safely
   * extract focusRingInfos[0].rects.
   * @param {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} focusRingInfos
   */
  setFocusRings: focusRingInfos => {
    MockAccessibilityPrivate.focusRings_ = focusRingInfos;
  },

  /**
   * Sets highlights.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @param {string} color
   */
  setHighlights: (rects, color) => {
    MockAccessibilityPrivate.highlightRects_ = rects;
    MockAccessibilityPrivate.highlightColor_ = color;
  },

  /**
   * Updates properties of the Select-to-speak panel.
   * @param {boolean} show
   * @param {!chrome.accessibilityPrivate.ScreenRect=} anchor
   * @param {boolean=} isPaused
   * @param {number=} speed
   */
  updateSelectToSpeakPanel: (show, anchor, isPaused, speed) => {
    MockAccessibilityPrivate
        .selectToSpeakPanelState_ = {show, anchor, isPaused, speed};
  },

  /** Called in order to toggle Dictation listening. */
  toggleDictation: () => {
    MockAccessibilityPrivate.dictationActivated_ =
        !MockAccessibilityPrivate.dictationActivated_;

    MockAccessibilityPrivate.callOnToggleDictation(
        MockAccessibilityPrivate.dictationActivated_);
  },

  /**
   * Whether a feature is enabled. This doesn't look at command line flags; set
   * enabled state with MockAccessibilityPrivate::enableFeatureForTest.
   * @param {AccessibilityFeature} feature
   * @param {function(boolean): void} callback
   */
  isFeatureEnabled(feature, callback) {
    callback(this.enabledFeatures_.has(feature));
  },

  /**
   * Creates a synthetic keyboard event.
   * @param {Object} unused
   */
  sendSyntheticKeyEvent(unused) {},

  // Methods for testing. //

  /**
   * Called to get the AccessibilityCommon extension to use the Automation
   * API to find the scrollable bounds at a point. In Automatic Clicks, this
   * would actually be initiated by ash/autoclick/autoclick_controller
   * calling the AccessibilityPrivate API call. When the bounds are found,
   * handleScrollableBoundsForPointFoundCallback will be called to inform
   * the test that work is complete.
   * @param {number} x
   * @param {number} y
   * @param {!function<>} handleScrollableBoundsForPointFoundCallback
   */
  callOnScrollableBoundsForPointRequested:
      (x, y, handleScrollableBoundsForPointFoundCallback) => {
        MockAccessibilityPrivate.handleScrollableBoundsForPointFoundCallback_ =
            handleScrollableBoundsForPointFoundCallback;
        MockAccessibilityPrivate.boundsListener_(x, y);
      },

  /**
   * Called to register a stubbed callback for moveMagnifierToRect.
   * When magnifier identifies a desired rect to move the viewport to,
   * moveMagnifierToRectCallback will be called with that desired rect.
   * @param {!function<>} moveMagnifierToRectCallback
   */
  registerMoveMagnifierToRectCallback: moveMagnifierToRectCallback => {
    MockAccessibilityPrivate.moveMagnifierToRectCallback_ =
        moveMagnifierToRectCallback;
  },

  /**
   * Gets the scrollable bounds which were found by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getScrollableBounds: () => {
    return MockAccessibilityPrivate.scrollableBounds_;
  },

  /**
   * Gets the focus rings bounds which were set by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.accessibilityPrivate.FocusRingInfo>}
   */
  getFocusRings: () => {
    return MockAccessibilityPrivate.focusRings_;
  },

  /**
   * Gets the highlight bounds.
   * @return {!Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getHighlightRects: () => {
    return MockAccessibilityPrivate.highlightRects_;
  },

  /**
   * Gets the color of the last highlight created.
   * @return {?string}
   */
  getHighlightColor: () => {
    return MockAccessibilityPrivate.highlightColor_;
  },

  /**
   * @return {?SelectToSpeakPanelState}
   */
  getSelectToSpeakPanelState: () => {
    return MockAccessibilityPrivate.selectToSpeakPanelState_;
  },

  /**
   * Simulates Select-to-speak panel action.
   * @param {!chrome.accessibilityPrivate.SelectToSpeakPanelAction} action
   * @param {number=} value
   */
  sendSelectToSpeakPanelAction(action, value) {
    if (MockAccessibilityPrivate.selectToSpeakPanelActionListener_) {
      MockAccessibilityPrivate.selectToSpeakPanelActionListener_(action, value);
    }
  },

  /**
   * Simulates Select-to-speak state change request (tray button).
   */
  sendSelectToSpeakStateChangeRequest() {
    if (MockAccessibilityPrivate.selectToSpeakStateChangeListener_) {
      MockAccessibilityPrivate.selectToSpeakStateChangeListener_();
    }
  },

  /**
   * Simulates Dictation activation change from AccessibilityManager, which may
   * occur when the user or a chrome extension toggles Dictation active state.
   * @param {boolean} activated
   */
  callOnToggleDictation: activated => {
    MockAccessibilityPrivate.dictationActivated_ = activated;
    if (MockAccessibilityPrivate.dictationToggleListener_) {
      MockAccessibilityPrivate.dictationToggleListener_(activated);
    }
  },

  /**
   * Gets the current Dictation active state. This can be flipped when
   * MockAccessibilityPrivate.toggleDictation is called, and set when
   * MocakAccessibilityPrivate.callOnToggleDictation is called.
   * @returns {boolean} The current Dictation active state.
   */
  getDictationActive() {
    return MockAccessibilityPrivate.dictationActivated_;
  },

  /** @param {!chrome.accessibilityPrivate.DictationBubbleProperties} props */
  updateDictationBubble(props) {
    MockAccessibilityPrivate.dictationBubbleProps_ = props;
  },

  /** @return {!chrome.accessibilityPrivate.DictationBubbleProperties|null} */
  getDictationBubbleProps() {
    return MockAccessibilityPrivate.dictationBubbleProps_;
  },

  /** Simulates silencing ChromeVox */
  silenceSpokenFeedback() {
    this.spokenFeedbackSilenceCount_++;
  },

  /** @return {number} */
  getSpokenFeedbackSilencedCount() {
    return this.spokenFeedbackSilenceCount_;
  },

  /**
   * Enables or disables a feature for testing, causing
   * MockAccessibilityPrivate.isFeatureEnabled to consider it enabled.
   * @param {AccessibilityFeature} feature
   * @param {boolean} enabled
   */
  enableFeatureForTest(feature, enabled) {
    if (enabled) {
      this.enabledFeatures_.add(feature);
    } else if (this.enabledFeatures_.has(feature)) {
      this.enabledFeatures_.delete(feature);
    }
  },
};
