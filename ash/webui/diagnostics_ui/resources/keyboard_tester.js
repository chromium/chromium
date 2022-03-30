// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {MechanicalLayout as DiagramMechanicalLayout, PhysicalLayout as DiagramPhysicalLayout, TopRightKey as DiagramTopRightKey, TopRowKey as DiagramTopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputDataProviderInterface, KeyboardInfo, KeyboardObserverInterface, KeyboardObserverReceiver, KeyEvent, KeyEventType, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from './diagnostics_types.js';

/**
 * @fileoverview
 * 'keyboard-tester' displays a tester UI for a keyboard.
 */

/**
 * Map from Mojo TopRowKey constants to keyboard diagram top row key
 * definitions.
 */
const topRowKeyMap = {
  [TopRowKey.kNone]: DiagramTopRowKey.kNone,
  [TopRowKey.kBack]: DiagramTopRowKey.kBack,
  [TopRowKey.kForward]: DiagramTopRowKey.kForward,
  [TopRowKey.kRefresh]: DiagramTopRowKey.kRefresh,
  [TopRowKey.kFullscreen]: DiagramTopRowKey.kFullscreen,
  [TopRowKey.kOverview]: DiagramTopRowKey.kOverview,
  [TopRowKey.kScreenshot]: DiagramTopRowKey.kScreenshot,
  [TopRowKey.kScreenBrightnessDown]: DiagramTopRowKey.kScreenBrightnessDown,
  [TopRowKey.kScreenBrightnessUp]: DiagramTopRowKey.kScreenBrightnessUp,
  [TopRowKey.kPrivacyScreenToggle]: DiagramTopRowKey.kPrivacyScreenToggle,
  [TopRowKey.kVolumeMute]: DiagramTopRowKey.kVolumeMute,
  [TopRowKey.kVolumeDown]: DiagramTopRowKey.kVolumeDown,
  [TopRowKey.kVolumeUp]: DiagramTopRowKey.kVolumeUp,
  [TopRowKey.kKeyboardBacklightDown]: DiagramTopRowKey.kKeyboardBacklightDown,
  [TopRowKey.kKeyboardBacklightUp]: DiagramTopRowKey.kKeyboardBacklightUp,
  [TopRowKey.kNextTrack]: DiagramTopRowKey.kNextTrack,
  [TopRowKey.kPreviousTrack]: DiagramTopRowKey.kPreviousTrack,
  [TopRowKey.kPlayPause]: DiagramTopRowKey.kPlayPause,
  [TopRowKey.kScreenMirror]: DiagramTopRowKey.kScreenMirror,
  [TopRowKey.kDelete]: DiagramTopRowKey.kDelete,
  [TopRowKey.kUnknown]: DiagramTopRowKey.kUnknown,
};

/** Maps top-right key evdev codes to the corresponding DiagramTopRightKey. */
const topRightKeyByCode = new Map([
  [116, DiagramTopRightKey.kPower],
  [142, DiagramTopRightKey.kLock],
  [579, DiagramTopRightKey.kControlPanel],
]);

Polymer({
  is: 'keyboard-tester',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?KeyboardObserverReceiver} */
  receiver_: null,

  /** @private {?InputDataProviderInterface} */
  inputDataProvider_: null,

  /**
   * Set the InputDataProvider to get events from.
   * @param {!InputDataProviderInterface} provider
   */
  setInputDataProvider(provider) {
    this.inputDataProvider_ = provider;
  },

  properties: {
    /**
     * The keyboard being tested, or null if none is being tested at the moment.
     * @type {?KeyboardInfo}
     */
    keyboard: KeyboardInfo,

    /** @private */
    layoutIsKnown_: {
      type: Boolean,
      computed: 'computeLayoutIsKnown_(keyboard)',
    },

    // TODO(crbug.com/1257138): use the proper type annotation instead of
    // string.
    /** @private {?string} */
    diagramMechanicalLayout_: {
      type: String,
      computed: 'computeDiagramMechanicalLayout_(keyboard)',
    },

    // TODO(crbug.com/1257138): use the proper type annotation instead of
    // string.
    /** @private {?string} */
    diagramPhysicalLayout_: {
      type: String,
      computed: 'computeDiagramPhysicalLayout_(keyboard)',
    },

    // TODO(crbug.com/1257138): use the proper type annotation instead of
    // string.
    /** @protected {?string} */
    diagramTopRightKey_: {
      type: String,
      computed: 'computeDiagramTopRightKey_(keyboard)',
    },

    /** @private */
    showNumberPad_: {
      type: Boolean,
      computed: 'computeShowNumberPad_(keyboard)',
    },

    // TODO(crbug.com/1257138): use the proper type annotation instead of
    // Object.
    /** @private {!Array<!Object>} */
    topRowKeys_: {
      type: Array,
      computed: 'computeTopRowKeys_(keyboard)',
    },
  },

  /**
   * @param {?KeyboardInfo} keyboard
   * @return {boolean}
   * @private
   */
  computeLayoutIsKnown_(keyboard) {
    if (!keyboard) {
      return false;
    }
    return keyboard.physicalLayout !== PhysicalLayout.kUnknown &&
        keyboard.mechanicalLayout !== MechanicalLayout.kUnknown;
    // Number pad presence can be unknown, as we can adapt on the fly if we get
    // a number pad event we weren't expecting.
  },

  /**
   * @param {?KeyboardInfo} keyboardInfo
   * TODO(crbug.com/1257138): use the proper type annotation instead of string.
   * @return {?string}
   * @private
   */
  computeDiagramMechanicalLayout_(keyboardInfo) {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [MechanicalLayout.kUnknown]: null,
      [MechanicalLayout.kAnsi]: DiagramMechanicalLayout.kAnsi,
      [MechanicalLayout.kIso]: DiagramMechanicalLayout.kIso,
      [MechanicalLayout.kJis]: DiagramMechanicalLayout.kJis,
    }[keyboardInfo.mechanicalLayout];
  },

  /**
   * @param {?KeyboardInfo} keyboardInfo
   * TODO(crbug.com/1257138): use the proper type annotation instead of string.
   * @return {?string}
   * @private
   */
  computeDiagramPhysicalLayout_(keyboardInfo) {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [PhysicalLayout.kUnknown]: null,
      [PhysicalLayout.kChromeOS]: DiagramPhysicalLayout.kChromeOS,
      [PhysicalLayout.kChromeOSDellEnterpriseWilco]:
          DiagramPhysicalLayout.kChromeOSDellEnterpriseWilco,
      [PhysicalLayout.kChromeOSDellEnterpriseDrallion]:
          DiagramPhysicalLayout.kChromeOSDellEnterpriseDrallion,
    }[keyboardInfo.physicalLayout];
  },

  /**
   * @param {?KeyboardInfo} keyboardInfo
   * TODO(crbug.com/1257138): use the proper type annotation instead of string.
   * @return {?string}
   * @private
   */
  computeDiagramTopRightKey_(keyboardInfo) {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [TopRightKey.kUnknown]: null,
      [TopRightKey.kPower]: DiagramTopRightKey.kPower,
      [TopRightKey.kLock]: DiagramTopRightKey.kLock,
      [TopRightKey.kControlPanel]: DiagramTopRightKey.kControlPanel,
    }[keyboardInfo.topRightKey];
  },

  /**
   * @param {?KeyboardInfo} keyboard
   * @return {boolean}
   * @private
   */
  computeShowNumberPad_(keyboard) {
    return !!keyboard &&
        keyboard.numberPadPresent === NumberPadPresence.kPresent;
  },


  /**
   * @param {?KeyboardInfo} keyboard
   * @return {!Array<!Object>}
   * @private
   */
  computeTopRowKeys_(keyboard) {
    if (!keyboard) {
      return [];
    }
    return keyboard.topRowKeys.map((keyId) => topRowKeyMap[keyId]);
  },

  /** Shows the tester's dialog. */
  show() {
    assert(this.inputDataProvider_);
    this.receiver_ = new KeyboardObserverReceiver(
        /** @type {!KeyboardObserverInterface} */ (this));
    this.inputDataProvider_.observeKeyEvents(
        this.keyboard.id, this.receiver_.$.bindNewPipeAndPassRemote());
    this.$.dialog.showModal();
  },

  /**
   * Returns whether the tester is currently open.
   * @return {boolean}
   */
  isOpen() {
    return this.$.dialog.open;
  },

  close() {
    this.$.dialog.close();
  },

  handleClose() {
    this.receiver_.$.close();
  },

  /**
   * Implements KeyboardObserver.OnKeyEvent.
   * @param {!KeyEvent} keyEvent
   */
  onKeyEvent(keyEvent) {
    const diagram = this.$$('#diagram');
    const state = keyEvent.type === KeyEventType.kPress ?
        KeyboardKeyState.kPressed :
        KeyboardKeyState.kTested;
    if (keyEvent.topRowPosition !== -1 &&
        keyEvent.topRowPosition < this.keyboard.topRowKeys.length) {
      diagram.setTopRowKeyState(keyEvent.topRowPosition, state);
    } else {
      // We can't be sure that the top right key reported over Mojo is correct,
      // so we need to fix it if we see a key event that suggests it's wrong.
      if (topRightKeyByCode.has(keyEvent.keyCode) &&
          diagram.topRightKey !== topRightKeyByCode.get(keyEvent.keyCode)) {
        const newValue = topRightKeyByCode.get(keyEvent.keyCode);
        console.warn(
            'Corrected diagram top right key from ' +
            `${this.diagramTopRightKey_} to ${newValue}`);
        diagram.topRightKey = newValue;
      }

      // Some Chromebooks (at least the Lenovo ThinkPad C13 Yoga a.k.a.
      // Morphius) report F13 instead of SLEEP when Lock is pressed.
      if (keyEvent.keyCode === 183 /* KEY_F13 */) {
        keyEvent.keyCode = 142 /* KEY_SLEEP */;
      }

      diagram.setKeyState(keyEvent.keyCode, state);
    }
  },

  /**
   * Implements KeyboardObserver.OnKeyEventsPaused.
   */
  onKeyEventsPaused() {
    // TODO(crbug.com/1207678): show key event pauses in the UI.
    console.log('key events paused');
    this.$$('#diagram').clearPressedKeys();
  },

  /**
   * Implements KeyboardObserver.OnKeyEventsResumed.
   */
  onKeyEventsResumed() {
    // TODO(crbug.com/1207678): show key event pauses in the UI.
    console.log('key events resumed');
  },
});
