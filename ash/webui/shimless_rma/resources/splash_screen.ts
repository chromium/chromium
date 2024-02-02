// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';
import './icons.html.js';
import './shimless_rma_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './splash_screen.html.js';

/**
 * @fileoverview
 * 'splash-screen' is displayed while waiting for the first state to be fetched
 * by getCurrentState.
 */

const SplashScreenBase = I18nMixin(PolymerElement);

export class SplashScreen extends SplashScreenBase {
  static get is() {
    return 'splash-screen' as const;
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * Display the splash instructions.
   */
  protected getSplashInstructionsText(): string {
    return this.i18n('shimlessSplashRemembering');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SplashScreen.is]: SplashScreen;
  }
}

customElements.define(SplashScreen.is, SplashScreen);
