// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './sign_in_promo.js';

import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './dice_app.html.js';

export interface IntroAppElement {
  $: {viewManager: CrViewManagerElement};
}

export class IntroAppElement extends PolymerElement {
  static get is() {
    return 'intro-app';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setupViewManager_(new URLSearchParams(window.location.search));
  }

  private async setupViewManager_(queryParams: URLSearchParams) {
    if (!queryParams.has('noAnimations')) {
      this.$.viewManager.switchView('splash', 'fade-in', 'fade-out');

      // TODO(crbug.com/1347507): Delay the switch to signInPromo based on the
      // splash animation timing instead.
      await new Promise(resolve => setTimeout(resolve, 1000));
    }

    this.$.viewManager.switchView('signInPromo', 'fade-in', 'no-animation');
  }

  async setupViewManagerForTest(queryParams: URLSearchParams) {
    await (this.setupViewManager_(queryParams));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': IntroAppElement;
  }
}

customElements.define(IntroAppElement.is, IntroAppElement);
