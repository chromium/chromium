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

/**
 * @typedef {{
 *  js_pumpkin_tagger_bin_js: !ArrayBuffer,
 *  tagger_wasm_main_js: !ArrayBuffer,
 *  tagger_wasm_main_wasm: !ArrayBuffer,
 *  en_us_action_config_binarypb: !ArrayBuffer,
 *  en_us_pumpkin_config_binarypb: !ArrayBuffer,
 *  fr_fr_action_config_binarypb: !ArrayBuffer,
 *  fr_fr_pumpkin_config_binarypb: !ArrayBuffer,
 *  it_it_action_config_binarypb: !ArrayBuffer,
 *  it_it_pumpkin_config_binarypb: !ArrayBuffer,
 *  de_de_action_config_binarypb: !ArrayBuffer,
 *  de_de_pumpkin_config_binarypb: !ArrayBuffer,
 *  es_es_action_config_binarypb: !ArrayBuffer,
 *  es_es_pumpkin_config_binarypb: !ArrayBuffer,
 * }}
 */
let MockPumpkinData;

/*
 * A mock AccessibilityPrivate API for tests.
 */
class MockAccessibilityPrivate {
  constructor() {
    this.FocusType = {
      SOLID: 'solid',
    };

    this.AccessibilityFeature = {
      DICTATION_CONTEXT_CHECKING: 'dictationContextChecking',
    };

    this.AssistiveTechnologyType = {
      CHROME_VOX: 'chromeVox',
      SELECT_TO_SPEAK: 'selectToSpeak',
      SWITCH_ACCESS: 'switchAccess',
      AUTO_CLICK: 'autoClick',
      MAGNIFIER: 'magnifier',
      DICTATION: 'dictation',
    };

    this.DictationBubbleIconType = {
      HIDDEN: 'hidden',
      STANDBY: 'standby',
      MACRO_SUCCESS: 'macroSuccess',
      MACRO_FAIL: 'macroFail',
    };

    this.DictationBubbleHintType = {
      TRY_SAYING: 'trySaying',
      TYPE: 'type',
      DELETE: 'delete',
      SELECT_ALL: 'selectAll',
      UNDO: 'undo',
      HELP: 'help',
      UNSELECT: 'unselect',
      COPY: 'copy',
    };

    this.ScrollDirection = {
      UP: 'up',
      DOWN: 'down',
      LEFT: 'left',
      RIGHT: 'right',
    };

    this.SelectToSpeakPanelAction = {
      PREVIOUS_PARAGRAPH: 'previousParagraph',
      PREVIOUS_SENTENCE: 'previousSentence',
      PAUSE: 'pause',
      RESUME: 'resume',
      NEXT_SENTENCE: 'nextSentence',
      NEXT_PARAGRAPH: 'nextParagraph',
      EXIT: 'exit',
      CHANGE_SPEED: 'changeSpeed',
    };

    this.SyntheticKeyboardEventType = {KEYDOWN: 'keydown', KEYUP: 'keyup'};

    this.ToastType = {
      DICTATION_MIC_MUTED: 'dictationMicMuted',
      DICTATION_NO_FOCUSED_TEXT_FIELD: 'dictationNoFocusedTextField',
    };

    this.SyntheticMouseEventType = {
      PRESS: 'press',
      RELEASE: 'release',
      DRAG: 'drag',
      MOVE: 'move',
      ENTER: 'enter',
      EXIT: 'exit',
    };

    this.SyntheticMouseEventButton = {
      LEFT: 'left',
      MIDDLE: 'middle',
      RIGHT: 'right',
      BACK: 'back',
      FOWARD: 'foward',
    };

    /** @private {function<number, number>} */
    this.boundsListener_ = null;

    /** @private {?MockPumpkinData} */
    this.pumpkinData_ = null;

    /** @private {?FaceGazeAssets} */
    this.faceGazeAssets_ = null;

    /** @private {function<boolean>} */
    this.faceGazeGestureInfoToggleListener_ = null;

    /** @private {number} */
    this.sendGestureInfoToSettingsCount_ = 0;

    /** @private {!Array<!chrome.accessibilityPrivate.GestureInfo>} */
    this.faceGazeGestureInfo_ = null;

    /**
     * @private {function(!chrome.accessibilityPrivate.SelectToSpeakPanelAction,
     *     number=)}
     */
    this.selectToSpeakPanelActionListener_ = null;

    /** @private {function()} */
    this.selectToSpeakStateChangeListener_ = null;

    /** @private {!chrome.accessibilityPrivate.ScreenRect} */
    this.scrollableBounds_ = {};

    /** @private {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} */
    this.focusRings_ = [];
    this.handleScrollableBoundsForPointFoundCallback_ = null;
    this.moveMagnifierToRectCallback_ = null;

    /** @private {?SelectToSpeakPanelState} */
    this.selectToSpeakPanelState_ = null;

    /** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
    this.highlightRects_ = [];

    /** @private {?string} */
    this.highlightColor_ = null;

    /** @private {!chrome.accessibilityPrivate.ScreenRect} */
    this.selectToSpeakFocus_ = null;

    /** @private {function<boolean>} */
    this.dictationToggleListener_ = null;

    /** @private {boolean} */
    this.dictationActivated_ = false;

    /**
     * @private {!chrome.accessibilityPrivate.DictationBubbleProperties|null}
     */
    this.dictationBubbleProps_ = null;

    /** @private {?string} */
    this.faceGazeBubbleText_ = null;

    /** @private {Function} */
    this.onUpdateDictationBubble_ = null;

    /** @private {Set<string>} */
    this.enabledFeatures_ = new Set();

    /** @private {number} */
    this.spokenFeedbackSilenceCount_ = 0;

    /** @private {!Object<chrome.accessibilityPrivate.ToastType, number} */
    this.showToastData_ = {};

    /**
     * @type {!{count: number, target: ScreenPoint, direction: ScrollDirection}}
     * @private
     */
    this.scrollAtPointData_ = {
      count: 0,
      target: undefined,
      direction: undefined,
    };

    /** @private {?chrome.accessibilityPrivate.ScreenPoint} */
    this.latestCursorPosition_ = null;

    /** @private {!Array<chrome.accessibilityPrivate.ScreenRect>} */
    this.displayBounds_ = [{left: 0, top: 0, width: 1200, height: 800}];

    /** @private {!Array<chrome.accessibilityPrivate.SyntheticMouseEvent> */
    this.syntheticMouseEvents_ = [];

    /** @private {!Array<chrome.accessibilityPrivate.SyntheticKeyboardEvent>} */
    this.syntheticKeyEvents_ = [];

    // Methods from AccessibilityPrivate API. //

    this.onScrollableBoundsForPointRequested = {
      /**
       * Adds a listener to onScrollableBoundsForPointRequested.
       * @param {function<number, number>} listener
       */
      addListener: listener => {
        this.boundsListener_ = listener;
      },

      /**
       * Removes the listener.
       * @param {function<number, number>} listener
       */
      removeListener: listener => {
        if (this.boundsListener_ === listener) {
          this.boundsListener_ = null;
        }
      },
    };

    this.onMagnifierBoundsChanged = {
      addListener: listener => {},
      removeListener: listener => {},
    };

    this.onChromeVoxFocusChanged = {
      addListener: listener => {},
      removeListener: listener => {},
    };

    this.onSelectToSpeakFocusChanged = {
      addListener: listener => {},
      removeListener: listener => {},
    };

    this.onSelectToSpeakPanelAction = {
      /**
       * Adds a listener to onSelectToSpeakPanelAction.
       * @param {!function(!chrome.accessibilityPrivate.SelectToSpeakPanelAction,
       *     number=)} listener
       */
      addListener: listener => {
        this.selectToSpeakPanelActionListener_ = listener;
      },
    };

    this.onToggleDictation = {
      /**
       * Adds a listener to onToggleDictation.
       * @param {function<boolean>} listener
       */
      addListener: listener => {
        this.dictationToggleListener_ = listener;
      },

      /**
       * Removes the listener.
       * @param {function<boolean>} listener
       */
      removeListener: listener => {
        if (this.dictationToggleListener_ === listener) {
          this.dictationToggleListener_ = null;
        }
      },
    };

    this.onSelectToSpeakStateChangeRequested = {
      /**
       * Adds a listener to onSelectToSpeakStateChangeRequested.
       * @param {!function()} listener
       */
      addListener: listener => {
        this.selectToSpeakStateChangeListener_ = listener;
      },
    };

    this.onToggleGestureInfoForSettings = {
      /**
       * Adds a listener to onToggleGestureInfoForSettings.
       * @param {function<boolean>} listener
       */
      addListener: listener => {
        this.faceGazeGestureInfoToggleListener_ = listener;
      },

      /**
       * Removes the listener.
       * @param {function<boolean>} listener
       */
      removeListener: listener => {
        if (this.faceGazeGestureInfoToggleListener_ === listener) {
          this.faceGazeGestureInfoToggleListener_ = null;
        }
      },
    };
  }

  /**
   * Called when AccessibilityCommon wants to enable mouse events.
   * @param {boolean} enabled
   */
  enableMouseEvents(enabled) {}

  /**
   * Called when AccessibilityCommon finds scrollable bounds at a point.
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   */
  handleScrollableBoundsForPointFound(bounds) {
    this.scrollableBounds_ = bounds;
    this.handleScrollableBoundsForPointFoundCallback_();
  }

  /**
   * Called when AccessibilityCommon wants to move the magnifier viewport to
   * include a specific rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   */
  moveMagnifierToRect(rect) {
    if (this.moveMagnifierToRectCallback_) {
      this.moveMagnifierToRectCallback_(rect);
    }
  }

  /**
   * Called when AccessibilityCommon wants to set the focus rings. We can
   * assume that it is only setting one set of rings at a time, and safely
   * extract focusRingInfos[0].rects.
   * @param {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} focusRingInfos
   * @param {chrome.accessibilityPrivate.AssistiveTechnologyType} atType
   */
  setFocusRings(focusRingInfos, atType) {
    this.focusRings_ = focusRingInfos;
  }

  /**
   * Sets highlights.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @param {string} color
   */
  setHighlights(rects, color) {
    this.highlightRects_ = rects;
    this.highlightColor_ = color;
  }

  /**
   * Updates properties of the Select-to-speak panel.
   * @param {boolean} show
   * @param {!chrome.accessibilityPrivate.ScreenRect=} anchor
   * @param {boolean=} isPaused
   * @param {number=} speed
   */
  updateSelectToSpeakPanel(show, anchor, isPaused, speed) {
    this.selectToSpeakPanelState_ = {show, anchor, isPaused, speed};
  }

  /**
   * Sets the Select to Speak reading focus.
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   */
  setSelectToSpeakFocus(bounds) {
    this.selectToSpeakFocus_ = bounds;
  }

  /** Called in order to toggle Dictation listening. */
  toggleDictation() {
    this.dictationActivated_ = !this.dictationActivated_;
    this.callOnToggleDictation(this.dictationActivated_);
  }

  /**
   * Whether a feature is enabled. This doesn't look at command line flags; set
   * enabled state with MockAccessibilityPrivate::enableFeatureForTest.
   * @param {AccessibilityFeature} feature
   * @param {function(boolean): void} callback
   */
  isFeatureEnabled(feature, callback) {
    callback(this.enabledFeatures_.has(feature));
  }

  /**
   * Creates a synthetic keyboard event.
   * @param {chrome.accessibilityPrivate.SyntheticKeyboardEvent} event
   */
  sendSyntheticKeyEvent(event) {
    this.syntheticKeyEvents_.push(event);
  }

  /** @return {?PumpkinData} */
  installPumpkinForDictation(callback) {
    callback(MockAccessibilityPrivate.pumpkinData_);
  }

  /** @return {?FaceGazeAssets} */
  installFaceGazeAssets(callback) {
    callback(this.faceGazeAssets_);
  }

  /** Called in order to toggle FaceGaze gesture info for settings. */
  toggleGestureInfoForSettings(enabled) {
    this.callOnToggleGestureInfoForSettings(enabled);
  }

  sendGestureInfoToSettings(gestureInfo) {
    this.callSendGestureInfoToSettings(gestureInfo);
  }

  /** @param {!chrome.accessibilityPrivate.ScreenPoint} point */
  setCursorPosition(point) {
    this.latestCursorPosition_ = point;
  }

  /** @param {!chrome.accessibilityPrivate.SyntheticMouseEvent} event */
  sendSyntheticMouseEvent(event) {
    this.syntheticMouseEvents_.push(event);
  }

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
  callOnScrollableBoundsForPointRequested(
      x, y, handleScrollableBoundsForPointFoundCallback) {
    this.handleScrollableBoundsForPointFoundCallback_ =
        handleScrollableBoundsForPointFoundCallback;
    this.boundsListener_(x, y);
  }

  /**
   * Called to register a stubbed callback for moveMagnifierToRect.
   * When magnifier identifies a desired rect to move the viewport to,
   * moveMagnifierToRectCallback will be called with that desired rect.
   * @param {!function<>} moveMagnifierToRectCallback
   */
  registerMoveMagnifierToRectCallback(moveMagnifierToRectCallback) {
    this.moveMagnifierToRectCallback_ = moveMagnifierToRectCallback;
  }

  /**
   * Gets the scrollable bounds which were found by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getScrollableBounds() {
    return this.scrollableBounds_;
  }

  /**
   * Gets the focus rings bounds which were set by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.accessibilityPrivate.FocusRingInfo>}
   */
  getFocusRings() {
    return this.focusRings_;
  }

  /**
   * Gets the highlight bounds.
   * @return {!Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getHighlightRects() {
    return this.highlightRects_;
  }

  clearHighlightRects() {
    this.highlightRects_ = [];
  }

  /**
   * Gets the color of the last highlight created.
   * @return {?string}
   */
  getHighlightColor() {
    return this.highlightColor_;
  }

  /**
   * @return {?SelectToSpeakPanelState}
   */
  getSelectToSpeakPanelState() {
    return this.selectToSpeakPanelState_;
  }

  /**
   * @return {?chrome.AccessibilityPrivate.ScreenRect}
   */
  getSelectToSpeakFocus() {
    return this.selectToSpeakFocus_;
  }

  clearSelectToSpeakFocus() {
    this.selectToSpeakFocus_ = null;
  }

  /**
   * Simulates Select-to-speak panel action.
   * @param {!chrome.accessibilityPrivate.SelectToSpeakPanelAction} action
   * @param {number=} value
   */
  sendSelectToSpeakPanelAction(action, value) {
    if (this.selectToSpeakPanelActionListener_) {
      this.selectToSpeakPanelActionListener_(action, value);
    }
  }

  /** Simulates Select-to-speak state change request (tray button). */
  sendSelectToSpeakStateChangeRequest() {
    if (this.selectToSpeakStateChangeListener_) {
      this.selectToSpeakStateChangeListener_();
    }
  }

  /**
   * Simulates Dictation activation change from AccessibilityManager, which may
   * occur when the user or a chrome extension toggles Dictation active state.
   * @param {boolean} activated
   */
  callOnToggleDictation(activated) {
    this.dictationActivated_ = activated;
    if (this.dictationToggleListener_) {
      this.dictationToggleListener_(activated);
    }
  }

  /**
   * Gets the current Dictation active state. This can be flipped when
   * this.toggleDictation is called, and set when
   * MocakAccessibilityPrivate.callOnToggleDictation is called.
   * @returns {boolean} The current Dictation active state.
   */
  getDictationActive() {
    return this.dictationActivated_;
  }

  /** @param {!Function} listener */
  addUpdateDictationBubbleListener(listener) {
    this.onUpdateDictationBubble_ = listener;
  }

  removeUpdateDictationBubbleListener() {
    this.onUpdateDictationBubble_ = null;
  }

  /** @param {!chrome.accessibilityPrivate.DictationBubbleProperties} props */
  updateDictationBubble(props) {
    this.dictationBubbleProps_ = props;
    if (this.onUpdateDictationBubble_) {
      this.onUpdateDictationBubble_();
    }
  }

  /** @param {string} text */
  updateFaceGazeBubble(text) {
    this.faceGazeBubbleText_ = text;
  }

  /** @return {!chrome.accessibilityPrivate.DictationBubbleProperties|null} */
  getDictationBubbleProps() {
    return this.dictationBubbleProps_;
  }

  /** @return {?string} */
  getFaceGazeBubbleText() {
    return this.faceGazeBubbleText_;
  }

  /**
   * Simulates toggling gesture info for FaceGaze Settings from
   * AccessibilityManager, which occurs when the user activates or deactivates
   * the page for FaceGaze gesture configuration settings.
   * @param {boolean} enabled
   */
  callOnToggleGestureInfoForSettings(enabled) {
    if (this.faceGazeGestureInfoToggleListener_) {
      this.faceGazeGestureInfoToggleListener_(enabled);
    }
  }

  /**
   * Simulates sending gesture info from FaceGaze to Settings.
   * @param {!Array<!chrome.accessibilityPrivate.GestureInfo>} gestureInfo
   *     Facial gestures.
   */
  callSendGestureInfoToSettings(gestureInfo) {
    this.sendGestureInfoToSettingsCount_++;
    this.faceGazeGestureInfo_ = gestureInfo;
  }

  /** @return {number} */
  getSendGestureInfoToSettingsCount() {
    return this.sendGestureInfoToSettingsCount_;
  }

  /** @return {!Array<!chrome.accessibilityPrivate.GestureInfo>} */
  getFaceGazeGestureInfo() {
    return this.faceGazeGestureInfo_;
  }

  /** Simulates silencing ChromeVox */
  silenceSpokenFeedback() {
    this.spokenFeedbackSilenceCount_++;
  }

  /** @return {number} */
  getSpokenFeedbackSilencedCount() {
    return this.spokenFeedbackSilenceCount_;
  }

  /** @return {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  getDisplayBounds(callback) {
    callback(this.displayBounds_);
  }

  /**
   * @param {!chrome.accessibilityPrivate.ToastType} type
   * @return {number}
   */
  getShowToastCount(type) {
    if (!this.showToastData_[type]) {
      return 0;
    }

    return this.showToastData_[type];
  }

  /** @return {?chrome.accessibilityPrivate.ScreenPoint} */
  getLatestCursorPosition() {
    return this.latestCursorPosition_;
  }

  getScrollAtPointCount() {
    return this.scrollAtPointData_.count;
  }

  getScrollAtPointTarget() {
    return this.scrollAtPointData_.target;
  }

  getScrollAtPointDirection() {
    return this.scrollAtPointData_.direction;
  }

  clearCursorPosition() {
    this.latestCursorPosition_ = null;
  }

  /** @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  setDisplayBounds(bounds) {
    this.displayBounds_ = bounds;
  }

  /**
   * Enables or disables a feature for testing, causing
   * this.isFeatureEnabled to consider it enabled.
   * @param {AccessibilityFeature} feature
   * @param {boolean} enabled
   */
  enableFeatureForTest(feature, enabled) {
    if (enabled) {
      this.enabledFeatures_.add(feature);
    } else if (this.enabledFeatures_.has(feature)) {
      this.enabledFeatures_.delete(feature);
    }
  }

  /** @return {!Promise} */
  async initializePumpkinData() {
    /**
     * @param {string} file
     * @return {!Promise<!ArrayBuffer>}
     */
    const getFileBytes = async (file) => {
      const response = await fetch(file);
      if (response.status === 404) {
        throw `Failed to fetch file: ${file}`;
      }

      return await response.arrayBuffer();
    };

    const data = {};
    const pumpkinDir = '../../accessibility_common/third_party/pumpkin';
    data.js_pumpkin_tagger_bin_js =
        await getFileBytes(`${pumpkinDir}/js_pumpkin_tagger_bin.js`);
    data.tagger_wasm_main_js =
        await getFileBytes(`${pumpkinDir}/tagger_wasm_main.js`);
    data.tagger_wasm_main_wasm =
        await getFileBytes(`${pumpkinDir}/tagger_wasm_main.wasm`);
    data.en_us_action_config_binarypb =
        await getFileBytes(`${pumpkinDir}/en_us/action_config.binarypb`);
    data.en_us_pumpkin_config_binarypb =
        await getFileBytes(`${pumpkinDir}/en_us/pumpkin_config.binarypb`);
    data.fr_fr_action_config_binarypb =
        await getFileBytes(`${pumpkinDir}/fr_fr/action_config.binarypb`);
    data.fr_fr_pumpkin_config_binarypb =
        await getFileBytes(`${pumpkinDir}/fr_fr/pumpkin_config.binarypb`);
    data.it_it_action_config_binarypb =
        await getFileBytes(`${pumpkinDir}/it_it/action_config.binarypb`);
    data.it_it_pumpkin_config_binarypb =
        await getFileBytes(`${pumpkinDir}/it_it/pumpkin_config.binarypb`);
    data.de_de_action_config_binarypb =
        await getFileBytes(`${pumpkinDir}/de_de/action_config.binarypb`);
    data.de_de_pumpkin_config_binarypb =
        await getFileBytes(`${pumpkinDir}/de_de/pumpkin_config.binarypb`);
    data.es_es_action_config_binarypb =
        await getFileBytes(`${pumpkinDir}/es_es/action_config.binarypb`);
    data.es_es_pumpkin_config_binarypb =
        await getFileBytes(`${pumpkinDir}/es_es/pumpkin_config.binarypb`);
    MockAccessibilityPrivate.pumpkinData_ = data;
  }

  /** @param {!chrome.accessibilityPrivate.ToastType} type */
  showToast(type) {
    if (!this.showToastData_[type]) {
      this.showToastData_[type] = 0;
    }
    this.showToastData_[type] += 1;
  }

  /** @return {!Promise} */
  async initializeFaceGazeAssets() {
    /**
     * @param {string} file
     * @return {!Promise<!ArrayBuffer>}
     */
    const getFileBytes = async (file) => {
      const response = await fetch(file);
      if (response.status === 404) {
        throw `Failed to fetch file: ${file}`;
      }

      return await response.arrayBuffer();
    };

    const assets = {};
    const mediapipeDir =
        '../../accessibility_common/third_party/mediapipe_task_vision';
    assets.model = await getFileBytes(`${mediapipeDir}/face_landmarker.task`);
    assets.wasm =
        await getFileBytes(`${mediapipeDir}/vision_wasm_internal.wasm`);
    this.faceGazeAssets_ = assets;
  }

  /**
   * @param {string} title
   * @param {string} description
   * @param {?string|undefined} cancelName
   * @param {function(boolean): void} callback
   */
  showConfirmationDialog(title, description, cancelName, callback) {}

  scrollAtPoint(target, direction) {
    this.scrollAtPointData_.count += 1;
    this.scrollAtPointData_.target = target;
    this.scrollAtPointData_.direction = direction;
  }
}
