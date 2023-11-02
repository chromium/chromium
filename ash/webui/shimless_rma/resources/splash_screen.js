// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';
import './icons.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'splash-screen' is displayed while waiting for the first state to be fetched
 * by getCurrentState.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SplashScreenBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SplashScreen extends SplashScreenBase {
  static get is() {
    return 'splash-screen';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * Display the splash instructions.
   * @returns {string}
   * @protected
   */
  getSplashInstructionsText_() {
    return this.i18n('shimlessSplashRemembering');
  }
}

customElements.define(SplashScreen.is, SplashScreen);
