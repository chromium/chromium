// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './side_panel_error_page.css.js';
import {getHtml} from './side_panel_error_page.html.js';

/*
 * Element responsible for rendering the error page in the side panel.
 */
export class SidePanelErrorPageElement extends CrLitElement {
  static get is() {
    return 'side-panel-error-page';
  }

  static override get properties() {
    return {
      isProtectedError: {
        type: Boolean,
        reflect: true,
      },
      darkMode: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Whether the error denoted by this page is a network/server error or a
  // protected page error.
  protected accessor isProtectedError: boolean = false;
  // Whether to render the side panel error page in dark mode.
  protected accessor darkMode: boolean = loadTimeData.getBoolean('darkMode');

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  setIsProtectedError(isProtectedError: boolean) {
    this.isProtectedError = isProtectedError;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'side-panel-error-page': SidePanelErrorPageElement;
  }
}

customElements.define(SidePanelErrorPageElement.is, SidePanelErrorPageElement);
