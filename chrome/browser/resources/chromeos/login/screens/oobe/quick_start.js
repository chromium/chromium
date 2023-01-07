// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying quick start screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the screen.
 * @enum {string}
 */
const QuickStartUIState = {
  LOADING: 'loading',
  VERIFICATION: 'verification',
  FIGURES: 'figures',
};

// Should be in sync with the C++ enum (ash::quick_start::Color).
const QuickStartColors = ['blue', 'red', 'green', 'yellow'];

// TODO(b/246697586) Figure out the right DPI.
// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;

// TODO(b/246698826) Figure out the dark light modes.
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const QuickStartScreenBase = Polymer.mixinBehaviors(
    [LoginScreenBehavior, MultiStepBehavior], Polymer.Element);

class QuickStartScreen extends QuickStartScreenBase {
  static get is() {
    return 'quick-start-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      figures_: Object,
      shapes_: {
        type: Object,
        // Should be in sync with the C++ enum (ash::quick_start::Shape).
        value: {CIRCLE: 0, DIAMOND: 1, TRIANGLE: 2, SQUARE: 3},
        readOnly: true,
      },
      canvasSize_: {
        type: Number,
        value: 0,
      },
    };
  }

  constructor() {
    super();
    this.UI_STEPS = QuickStartUIState;
    this.figures_ = [];
    this.canvasSize_ = 0;
  }

  get EXTERNAL_API() {
    return ['setFigures', 'setQRCode'];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('QuickStartScreen');
  }

  /** @override */
  defaultUIStep() {
    return QuickStartUIState.LOADING;
  }

  /**
   * @param {!Array<OobeTypes.QuickStartScreenFigureData>} figures
   */
  setFigures(figures) {
    this.setUIStep(QuickStartUIState.FIGURES);
    this.figures_ = figures.map(x => {
      return {shape: x.shape, color: QuickStartColors[x.color], digit: x.digit};
    });
  }

  /**
   * @param {!Array<boolean>} qrCode
   */
  setQRCode(qrCode) {
    const qrSize = Math.round(Math.sqrt(qrCode.length));
    this.setUIStep(QuickStartUIState.VERIFICATION);

    this.canvasSize_ = qrSize * QR_CODE_TILE_SIZE;
    Polymer.dom.flush();
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < qrSize; x++) {
      for (let y = 0; y < qrSize; y++) {
        if (qrCode[index]) {
          context.fillRect(
              x * QR_CODE_TILE_SIZE, y * QR_CODE_TILE_SIZE, QR_CODE_TILE_SIZE,
              QR_CODE_TILE_SIZE);
        }
        index++;
      }
    }
  }

  getCanvasContext_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas').getContext('2d');
  }

  onNextClicked_() {
    this.userActed('next');
  }

  isEq_(a, b) {
    return a === b;
  }
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
