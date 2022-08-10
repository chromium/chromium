// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {IntroBrowserProxy, IntroBrowserProxyImpl} from './browser_proxy.js';

export interface IntroAppElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export class IntroAppElement extends PolymerElement {
  static get is() {
    return 'intro-app';
  }

  static get template() {
    return getTemplate();
  }

  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.setupViewManager_();
  }

  private async setupViewManager_() {
    this.$.viewManager.switchView('splash', 'fade-in', 'fade-out');

    // TODO(crbug.com/1347507): Delay the switch to signInPromo based on the
    // splash animation timing instead.
    await new Promise(resolve => setTimeout(resolve, 1000));

    this.$.viewManager.switchView('signInPromo', 'fade-in', 'no-animation');
  }

  private onContinueWithAccountClick_() {
    this.browserProxy_.continueWithAccount();
  }

  private onContinueWithoutAccountClick_() {
    this.browserProxy_.continueWithoutAccount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': IntroAppElement;
  }
}

customElements.define(IntroAppElement.is, IntroAppElement);
