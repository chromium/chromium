// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './new_tab_button.css.js';
import {getHtml} from './new_tab_button.html.js';

export class NewTabButtonElement extends CrLitElement {
  static get is() {
    return 'new-tab-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onClick() {
    chrome.histograms.recordUserAction('MobileToolbarNewTab');
    // No position or URL: the tab is appended and opens the default new tab
    // page.
    BrowserProxy.getInstance().tabStripService.createTabAt(null, null);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-button': NewTabButtonElement;
  }
}

customElements.define(NewTabButtonElement.is, NewTabButtonElement);
