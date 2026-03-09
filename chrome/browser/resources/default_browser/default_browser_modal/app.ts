// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export class DefaultBrowserModalAppElement extends CrLitElement {
  static get is() {
    return 'default-browser-modal-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      useSettingsIllustration: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  override firstUpdated() {
    requestAnimationFrame(() => {
      // Prefer using `document.body.offsetHeight` instead of
      // `document.body.scrollHeight` as it returns the correct height of the
      // page even when the page zoom in Chrome is different than 100% or the
      // system display scale.
      if (!this.isModal_) {
        return;
      }
      BrowserProxy.getInstance().handler.contentReady(
          document.body.offsetHeight);
      // The web dialog size has been initialized, so reset the body width to
      // auto to make sure that the body only takes up the viewable width.
      document.body.style.width = 'auto';
    });
  }

  accessor useSettingsIllustration: boolean =
      loadTimeData.getBoolean('useSettingsIllustration');

  private isModal_: boolean = loadTimeData.getBoolean('isModal');

  protected onCancelClick_() {
    BrowserProxy.getInstance().handler.cancel();
  }

  protected onConfirmClick_() {
    BrowserProxy.getInstance().handler.confirm();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'default-browser-modal-app': DefaultBrowserModalAppElement;
  }
}

customElements.define(
    DefaultBrowserModalAppElement.is, DefaultBrowserModalAppElement);
