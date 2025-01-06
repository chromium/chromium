// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './keyboard_diagram.html.js';
import {KeyboardKeyState} from './keyboard_key.js';
import {getKeyboardLayoutForRegionCode} from './keyboard_layouts.js';

/**
 * @fileoverview
 * 'keyboard-diagram' displays a diagram of a CrOS-style keyboard.
 */

// Size ratios derived from diagrams in the Chromebook keyboard spec.
const HEIGHT_TO_WIDTH_RATIO = 663 / 1760;
const EXTENDED_HEIGHT_TO_WIDTH_RATIO = 9 / 31;

/** The minimum diagram height at which key glyphs are legible. */
const MINIMUM_HEIGHT_PX = 250;

/**
 * Enum of mechanical layouts supported by the component.
 * @enum {string}
 */
export const MechanicalLayout = {
  ANSI: 'ansi',
  ISO: 'iso',
  JIS: 'jis',
};

/**
 * Enum of physical styles supported by the component.
 * @enum {string}
 */
export const PhysicalLayout = {
  CHROME_OS: 'chrome-os',
  CHROME_OS_DELL_ENTERPRISE_WILCO: 'dell-enterprise-wilco',
  CHROME_OS_DELL_ENTERPRISE_DRALLION: 'dell-enterprise-drallion',
};

/**
 * Enum of top-right keys supported by the component.
 * @enum {string}
 */
export const TopRightKey = {
  POWER: 'power',
  LOCK: 'lock',
  CONTROL_PANEL: 'control-panel',
};

/**
 * Enum of action keys to be shown on the top row.
 * @enum {!Object<string,
 *                !{icon: ?string, text: ?string, ariaNameI18n: ?string}>}
 */
export const TopRowKey = {
  kNone: {},
  kBack: {icon: 'keyboard:back', ariaNameI18n: 'keyboardDiagramAriaNameBack'},
  kForward: {
    icon: 'keyboard:forward',
    ariaNameI18n: 'keyboardDiagramAriaNameForward',
  },
  kRefresh: {
    icon: 'keyboard:refresh',
    ariaNameI18n: 'keyboardDiagramAriaNameRefresh',
  },
  kFullscreen: {
    icon: 'keyboard:fullscreen',
    ariaNameI18n: 'keyboardDiagramAriaNameFullscreen',
  },
  kOverview: {
    icon: 'keyboard:overview',
    ariaNameI18n: 'keyboardDiagramAriaNameOverview',
  },
  kScreenshot: {
    icon: 'keyboard:screenshot',
    ariaNameI18n: 'keyboardDiagramAriaNameScreenshot',
  },
  kScreenBrightnessDown: {
    icon: 'keyboard:display-brightness-down',
    ariaNameI18n: 'keyboardDiagramAriaNameScreenBrightnessDown',
  },
  kScreenBrightnessUp: {
    icon: 'keyboard:display-brightness-up',
    ariaNameI18n: 'keyboardDiagramAriaNameScreenBrightnessUp',
  },
  kPrivacyScreenToggle: {
    icon: 'keyboard:electronic-privacy-screen',
    ariaNameI18n: 'keyboardDiagramAriaNamePrivacyScreenToggle',
  },
  kMicrophoneMute: {
    icon: 'keyboard:microphone-mute',
    ariaNameI18n: 'keyboardDiagramAriaNameMicrophoneMute',
  },
  kVolumeMute: {
    icon: 'keyboard:volume-mute',
    ariaNameI18n: 'keyboardDiagramAriaNameMute',
  },
  kVolumeDown: {
    icon: 'keyboard:volume-down',
    ariaNameI18n: 'keyboardDiagramAriaNameVolumeDown',
  },
  kVolumeUp: {
    icon: 'keyboard:volume-up',
    ariaNameI18n: 'keyboardDiagramAriaNameVolumeUp',
  },
  kKeyboardBacklightToggle: {
    icon: 'keyboard:keyboard-brightness-toggle',
    ariaNameI18n: 'keyboardDiagramAriaNameKeyboardBacklightToggle',
  },
  kKeyboardBacklightDown: {
    icon: 'keyboard:keyboard-brightness-down',
    ariaNameI18n: 'keyboardDiagramAriaNameKeyboardBacklightDown',
  },
  kKeyboardBacklightUp: {
    icon: 'keyboard:keyboard-brightness-up',
    ariaNameI18n: 'keyboardDiagramAriaNameKeyboardBacklightUp',
  },
  kNextTrack: {
    icon: 'keyboard:next-track',
    ariaNameI18n: 'keyboardDiagramAriaNameTrackNext',
  },
  kPreviousTrack: {
    icon: 'keyboard:last-track',
    ariaNameI18n: 'keyboardDiagramAriaNameTrackPrevious',
  },
  kPlayPause: {
    icon: 'keyboard:play-pause',
    ariaNameI18n: 'keyboardDiagramAriaNamePlayPause',
  },
  kScreenMirror: {
    icon: 'keyboard:screen-mirror',
    ariaNameI18n: 'keyboardDiagramAriaNameScreenMirror',
  },
  // TODO(crbug.com/1207678): work out the localization scheme for keys like
  // delete and unknown.
  kDelete: {text: 'delete'},
  kUnknown: {text: 'unknown'},
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const KeyboardDiagramElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class KeyboardDiagramElement extends KeyboardDiagramElementBase {
  static get is() {
    return 'keyboard-diagram';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The mechanical layout to be displayed, or null for the default.
       * @type {?MechanicalLayout}
       */
      mechanicalLayout: String,

      /**
       * The physical style of the keyboard to be displayed, or null for the
       * default.
       * @type {?PhysicalLayout}
       */
      physicalLayout: String,

      /**
       * For internal keyboards, the region code of the device, used to
       * determine the key labels.
       * @type {?string}
       */
      regionCode: {
        type: String,
        observer: 'regionCodeChanged_',
      },

      /** Whether to show the Assistant key (between Ctrl and Alt). */
      showAssistantKey: Boolean,

      /** Whether to show a Chrome OS-style number pad.  */
      showNumberPad: {
        type: Boolean,
        observer: 'updateHeight_',
      },

      /** @private {boolean} */
      showFnAndGlobeKeys_: {
        type: Boolean,
        computed: 'computeShowFnAndGlobeKeys_(physicalLayout)',
      },

      /**
       * The keys to display on the top row.
       * @type {!Array<!TopRowKey>}
       */
      topRowKeys: {
        type: Array,
        value: [],
      },

      /**
       * The icon to display on the top-right key.
       * @type {?TopRightKey}
       */
      topRightKey: {
        type: String,
        value: TopRightKey.LOCK,
      },

      /** @protected {number} */
      topRightKeyCode_: {
        type: Number,
        computed: 'computeTopRightKeyCode_(topRightKey)',
      },

      /** @protected {string} */
      topRightKeyIcon_: {
        type: String,
        computed: 'computeTopRightKeyIcon_(topRightKey)',
      },

      /** @protected {string} */
      topRightKeyAriaNameI18n_: {
        type: String,
        computed: 'computeTopRightKeyAriaNameI18n_(topRightKey)',
      },
    };
  }

  /**
   * @param {?PhysicalLayout} physicalLayout
   * @return {boolean}
   * @private
   */
  computeShowFnAndGlobeKeys_(physicalLayout) {
    return physicalLayout == PhysicalLayout.CHROME_OS_DELL_ENTERPRISE_WILCO ||
        physicalLayout == PhysicalLayout.CHROME_OS_DELL_ENTERPRISE_DRALLION;
  }

  /**
   * @param {?TopRightKey} topRightKey
   * @return {number}
   * @private
   */
  computeTopRightKeyCode_(topRightKey) {
    return {
      [TopRightKey.POWER]: 116,
      [TopRightKey.LOCK]: 142,
      [TopRightKey.CONTROL_PANEL]: 579,
    }[topRightKey];
  }

  /**
   * @param {?TopRightKey} topRightKey
   * @return {string}
   * @private
   */
  computeTopRightKeyIcon_(topRightKey) {
    return 'keyboard:' + topRightKey;
  }

  /**
   * @param {?TopRightKey} topRightKey
   * @return {string}
   * @private
   */
  computeTopRightKeyAriaNameI18n_(topRightKey) {
    return {
      [TopRightKey.POWER]: 'keyboardDiagramAriaNamePower',
      [TopRightKey.LOCK]: 'keyboardDiagramAriaNameLock',
      [TopRightKey.CONTROL_PANEL]: 'keyboardDiagramAriaNameControlPanel',
    }[topRightKey];
  }

  constructor() {
    super();

    /** @private */
    this.resizeObserver_ = new ResizeObserver(this.onResize_.bind(this));

    /** @private {?number} */
    this.currentWidth_ = null;
  }

  ready() {
    super.ready();

    // We have to observe the size of an element other than the keyboard itself,
    // to avoid ResizeObserver call loops when we change the width of the
    // keyboard element.
    this.resizeObserver_.observe(this.$.widthChangeDetector);
  }

  /**
   * Utility method for the HTML template to check values are equal.
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  }

  /**
   * Utility method for the HTML template to retrieve a localized string, that
   * returns null if the ID is null or undefined.
   * @param {?string} stringId The ID to retrieve.
   * @return {?string} The localized string, or null if stringId is null or
   *     undefined.
   * @protected
   */
  optionalI18n_(stringId) {
    if (!stringId) {
      return null;
    }
    return this.i18n(stringId);
  }

  /**
   * @param {?string} newValue
   * @param {?string} oldValue
   * @private
   */
  regionCodeChanged_(newValue, oldValue) {
    const layout = getKeyboardLayoutForRegionCode(newValue);
    if (!layout) {
      return;
    }

    for (const [evdevCode, glyphs] of layout) {
      // Exclude the lower part of the enter key, which has the data-code
      // attribute for an enter key but shouldn't be labelled.
      const keys = this.root.querySelectorAll(
          `:not(#enterKeyLowerPart)[data-code="${evdevCode}"]`);
      for (const key of keys) {
        if (typeof glyphs === 'string') {
          key.ariaName = null;
          key.topLeftGlyph = null;
          key.topRightGlyph = null;
          key.bottomLeftGlyph = null;
          key.bottomRightGlyph = null;
          key.icon = null;
          key.mainGlyph = glyphs;
        } else {
          key.topLeftGlyph = glyphs.topLeft;
          key.topRightGlyph = glyphs.topRight;
          key.bottomLeftGlyph = glyphs.bottomLeft;
          key.bottomRightGlyph = glyphs.bottomRight;
          key.icon = glyphs.icon;
          key.mainGlyph = glyphs.main;

          if (glyphs.ariaNameI18n) {
            key.ariaName = this.i18n(glyphs.ariaNameI18n);
          }
        }
      }
    }
  }

  /** @private */
  updateHeight_() {
    const width = this.$.keyboard.offsetWidth;
    const widthToHeightRatio = this.showNumberPad ?
        EXTENDED_HEIGHT_TO_WIDTH_RATIO :
        HEIGHT_TO_WIDTH_RATIO;
    const height = Math.max(width * widthToHeightRatio, MINIMUM_HEIGHT_PX);
    this.$.keyboard.style.height = `${height}px`;
  }

  /** @private */
  onResize_() {
    const newWidth = this.$.keyboard.offsetWidth;
    if (newWidth !== this.currentWidth_) {
      this.updateHeight_();
      this.currentWidth_ = newWidth;
    }
  }

  /**
   * Set the state of a given key.
   * @param {number} evdevCode
   * @param {!KeyboardKeyState} state
   */
  setKeyState(evdevCode, state) {
    const keys = this.root.querySelectorAll(`[data-code="${evdevCode}"]`);
    if (keys.length === 0) {
      console.warn(`No keys found for evdev code ${evdevCode}.`);
      return;
    }
    for (const key of keys) {
      key.state = state;
    }
  }

  /**
   * Set the state of a top row key.
   * @param {number} topRowPosition The position of the key on the top row,
   *     where 0 is the first key after escape (which is not counted as part of
   *     the top row).
   * @param {!KeyboardKeyState} state
   */
  setTopRowKeyState(topRowPosition, state) {
    if (topRowPosition < 0 || topRowPosition >= this.topRowKeys.length) {
      throw new RangeError(
          `Invalid top row position ${topRowPosition} ` +
          `>= ${this.topRowKeys.length}`);
    }
    this.$.topRow.children[topRowPosition + 1].state = state;
  }

  /** Set any pressed keys to the "tested" state. */
  clearPressedKeys() {
    const keys = this.root.querySelectorAll(
        `keyboard-key[state="${KeyboardKeyState.PRESSED}"]`);
    for (const key of keys) {
      key.state = KeyboardKeyState.TESTED;
    }
  }

  /** Set all keys to the "not pressed" state. */
  resetAllKeys() {
    const keys = this.root.querySelectorAll(`keyboard-key`);
    for (const key of keys) {
      key.state = KeyboardKeyState.NOT_PRESSED;
    }
  }
}

customElements.define(KeyboardDiagramElement.is, KeyboardDiagramElement);
