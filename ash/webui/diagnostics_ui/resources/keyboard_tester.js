// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {MechanicalLayout as DiagramMechanicalLayout, PhysicalLayout as DiagramPhysicalLayout, TopRightKey as DiagramTopRightKey, TopRowKey as DiagramTopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/cr_elements/i18n_behavior.js';
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
  [TopRowKey.kMicrophoneMute]: DiagramTopRowKey.kMicrophoneMute,
  [TopRowKey.kVolumeMute]: DiagramTopRowKey.kVolumeMute,
  [TopRowKey.kVolumeDown]: DiagramTopRowKey.kVolumeDown,
  [TopRowKey.kVolumeUp]: DiagramTopRowKey.kVolumeUp,
  [TopRowKey.kKeyboardBacklightToggle]:
      DiagramTopRowKey.kKeyboardBacklightToggle,
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

/** Evdev codes for keys that always appear in the number pad area. */
const numberPadCodes = new Set([
  55,   // KEY_KPASTERISK
  71,   // KEY_KP7
  72,   // KEY_KP8
  73,   // KEY_KP9
  74,   // KEY_KPMINUS
  75,   // KEY_KP4
  76,   // KEY_KP5
  77,   // KEY_KP6
  78,   // KEY_KPPLUS
  79,   // KEY_KP1
  80,   // KEY_KP2
  81,   // KEY_KP3
  82,   // KEY_KP0
  83,   // KEY_KPDOT
  96,   // KEY_KPENTER
  98,   // KEY_KPSLASH
  102,  // KEY_HOME
  107,  // KEY_END
]);

/**
 * Evdev codes for keys that appear in the number pad area on standard ChromeOS
 * keyboards, but not on Dell Enterprise ones.
 */
const standardNumberPadCodes = new Set([
  104,  // KEY_PAGEUP
  109,  // KEY_PAGEDOWN
  111,  // KEY_DELETE
]);

Polymer({
  is: 'keyboard-tester',

  created: function() {
    this.addEventListener('keydown', this.onKeyDown.bind(this));
    this.addEventListener('keyup', this.onKeyUp.bind(this));
  },

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

  onKeyUp(e) {
    e.preventDefault();
    e.stopPropagation();
  },

  onKeyDown(e) {
    e.preventDefault();
    e.stopPropagation();

    // If we receive alt + esc we should close the app
    if (e.altKey && e.key === 'Escape') {
      this.close();
    }
  },

  /**
   * Returns whether the tester is currently open.
   * @return {boolean}
   */
  isOpen() {
    return this.$.dialog.open;
  },

  close() {
    this.$$('#diagram').clearPressedKeys();
    this.$.dialog.close();
  },

  handleClose() {
    if (this.receiver_) {
      this.receiver_.$.close();
    }
  },

  /**
   * Returns whether a key is part of the number pad on this keyboard layout.
   * @param {number} evdevCode
   * @return {boolean}
   */
  isNumberPadKey_(evdevCode) {
    // Some keys that are on the number pad on standard ChromeOS keyboards are
    // elsewhere on Dell Enterprise keyboards, so we should only check them if
    // we know this is a standard layout.
    if (this.keyboard.physicalLayout === PhysicalLayout.kChromeOS &&
        standardNumberPadCodes.has(evdevCode)) {
      return true;
    }

    return numberPadCodes.has(evdevCode);
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

      // There may be Chromebooks where hasNumberPad is incorrect, so if we see
      // any number pad key codes we need to adapt on-the-fly.
      if (!diagram.showNumberPad && this.isNumberPadKey_(keyEvent.keyCode)) {
        console.warn(
            'Corrected number pad presence due to key code ' +
            keyEvent.keyCode);
        diagram.showNumberPad = true;
      }

      diagram.setKeyState(keyEvent.keyCode, state);
    }
  },

  /**
   * Implements KeyboardObserver.OnKeyEventsPaused.
   */
  onKeyEventsPaused() {
    console.log('Key events paused');
    this.$$('#diagram').clearPressedKeys();
    this.$.lostFocusToast.show();
  },

  /**
   * Implements KeyboardObserver.OnKeyEventsResumed.
   */
  onKeyEventsResumed() {
    console.log('Key events resumed');
    this.$.lostFocusToast.hide();
  },
});
