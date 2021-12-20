// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './keyboard_key.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'keyboard-diagram' displays a diagram of a CrOS-style keyboard.
 */

// Size ratios derived from diagrams in the Chromebook keyboard spec.
const HEIGHT_TO_WIDTH_RATIO = 663 / 1760;
const EXTENDED_HEIGHT_TO_WIDTH_RATIO = 9 / 31;

/**
 * Enum of mechanical layouts supported by the component.
 * @enum {string}
 */
export const MechanicalLayout = {
  kAnsi: 'ansi',
  kIso: 'iso',
  kJis: 'jis',
};

/**
 * Enum of physical styles supported by the component.
 * @enum {string}
 */
export const PhysicalLayout = {
  kChromeOS: 'chrome-os',
  kChromeOSDellEnterprise: 'dell-enterprise',
};

/**
 * Enum of action keys to be shown on the top row.
 * @enum {!Object<string, !{icon: ?string, text: ?string}>}
 */
export const TopRowKey = {
  kNone: {},
  kBack: {icon: 'keyboard:back'},
  kForward: {icon: 'keyboard:forward'},
  kRefresh: {icon: 'keyboard:refresh'},
  kFullscreen: {icon: 'keyboard:fullscreen'},
  kOverview: {icon: 'keyboard:overview'},
  kScreenshot: {icon: 'keyboard:screenshot'},
  kScreenBrightnessDown: {icon: 'keyboard:display-brightness-down'},
  kScreenBrightnessUp: {icon: 'keyboard:display-brightness-up'},
  kPrivacyScreenToggle: {icon: 'keyboard:electronic-privacy-screen'},
  kVolumeMute: {icon: 'keyboard:volume-mute'},
  kVolumeDown: {icon: 'keyboard:volume-down'},
  kVolumeUp: {icon: 'keyboard:volume-up'},
  kKeyboardBacklightDown: {icon: 'keyboard:keyboard-brightness-down'},
  kKeyboardBacklightUp: {icon: 'keyboard:keyboard-brightness-up'},
  kNextTrack: {icon: 'keyboard:next-track'},
  kPreviousTrack: {icon: 'keyboard:last-track'},
  kPlayPause: {icon: 'keyboard:play-pause'},
  kScreenMirror: {icon: 'keyboard:screen-mirror'},
  // TODO(crbug.com/1207678): work out the localization scheme for keys like
  // delete and unknown.
  kDelete: {text: 'delete'},
  kUnknown: {text: 'unknown'},
};

export class KeyboardDiagramElement extends PolymerElement {
  static get is() {
    return 'keyboard-diagram';
  }

  static get template() {
    return html`{__html_template__}`;
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

      /** Whether to show the Assistant key (between Ctrl and Alt). */
      showAssistantKey: Boolean,

      /** Whether to show a Chrome OS-style number pad.  */
      showNumberPad: {
        type: Boolean,
        observer: 'updateHeight_',
      },

      /**
       * The keys to display on the top row.
       * @type {!Array<!TopRowKey>}
       */
      topRowKeys: {
        type: Array,
        value: [],
      },
    };
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

  /** @private */
  updateHeight_() {
    const width = this.$.keyboard.offsetWidth;
    const widthToHeightRatio = this.showNumberPad ?
        EXTENDED_HEIGHT_TO_WIDTH_RATIO :
        HEIGHT_TO_WIDTH_RATIO;
    this.$.keyboard.style.height = `${width * widthToHeightRatio}px`;
  }

  /** @private */
  onResize_() {
    const newWidth = this.$.keyboard.offsetWidth;
    if (newWidth !== this.currentWidth_) {
      this.updateHeight_();
      this.currentWidth_ = newWidth;
    }
  }
}

customElements.define(KeyboardDiagramElement.is, KeyboardDiagramElement);
