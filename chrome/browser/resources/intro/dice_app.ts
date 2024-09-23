// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './sign_in_promo.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './dice_app.css.js';
import {getHtml} from './dice_app.html.js';

export interface DiceAppElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export class DiceAppElement extends CrLitElement {
  static get is() {
    return 'intro-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setupViewManager_(new URLSearchParams(window.location.search));
  }

  private async setupViewManager_(queryParams: URLSearchParams) {
    if (!queryParams.has('noAnimations')) {
      const kSplashViewDurationMillis = 1500;
      this.$.viewManager.switchView('splash', 'no-animation', 'no-animation');
      // Delay the switch to signInPromo based on the splash animation timing.
      await new Promise(
          resolve => setTimeout(resolve, kSplashViewDurationMillis));
    }
    this.$.viewManager.switchView(
        'signInPromo', 'no-animation', 'no-animation');
  }

  setupViewManagerForTest(queryParams: URLSearchParams) {
    return this.setupViewManager_(queryParams);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': DiceAppElement;
  }
}

customElements.define(DiceAppElement.is, DiceAppElement);
