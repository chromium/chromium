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
};

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
    return {};
  }

  constructor() {
    super();
    this.UI_STEPS = QuickStartUIState;
  }


  get EXTERNAL_API() {
    return [];
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
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
