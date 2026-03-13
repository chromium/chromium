// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './reopen_tabs.css.js';
import {getHtml} from './reopen_tabs.html.js';

export class ReopenTabsElement extends CrLitElement {
  static get is() {
    return 'reopen-tabs';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onReopenClick_() {
    this.fire('reopen-click');
  }

  protected onDismissClick_() {
    this.fire('dismiss-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reopen-tabs': ReopenTabsElement;
  }
}

customElements.define(ReopenTabsElement.is, ReopenTabsElement);
