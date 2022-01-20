// Copyright 2021 The Chromium Authors. All rights reserved.
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
};

// Should be in sync with the C++ enum (ash::quick_start::Color).
const QuickStartColors = ['blue', 'red', 'green', 'yellow'];

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
        readOnly: true
      },
    };
  }

  constructor() {
    super();
    this.UI_STEPS = QuickStartUIState;
    this.figures_ = [];
  }

  get EXTERNAL_API() {
    return ['setFigures'];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('QuickStartScreen', {
      resetAllowed: true,
    });
  }

  /** @override */
  defaultUIStep() {
    return QuickStartUIState.LOADING;
  }

  /**
   * @param {!Array<OobeTypes.QuickStartScreenFigureData>} figures
   */
  setFigures(figures) {
    this.setUIStep(QuickStartUIState.VERIFICATION);
    this.figures_ = figures.map(x => {
      return {shape: x.shape, color: QuickStartColors[x.color], digit: x.digit};
    });
  }

  onNextClicked_() {
    this.userActed('next');
  }

  isEq_(a, b) {
    return a === b;
  }
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
