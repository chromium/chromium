// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './supported_links_dialog.css.js';
import {getHtml} from './supported_links_dialog.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class SupportedLinksDialogElement extends CrLitElement {
  static get is() {
    return 'app-management-supported-links-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
    };
  }

  app: App = createDummyApp();

  override firstUpdated() {
    this.addEventListener(
        'keydown', e => this.trapDialogFocus_(e as KeyboardEvent));
  }

  // The close button is the only tabbable element in the dialog, so focus
  // should stay on it.
  private trapDialogFocus_(e: KeyboardEvent) {
    if (e.key === 'Tab') {
      e.preventDefault();
      const dialogElement = this.shadowRoot?.getElementById('dialog');
      const buttonElement =
          dialogElement?.shadowRoot?.querySelector<CrIconButtonElement>(
              '#close');
      if (buttonElement) {
        buttonElement.focus();
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-supported-links-dialog': SupportedLinksDialogElement;
  }
}

customElements.define(
    SupportedLinksDialogElement.is, SupportedLinksDialogElement);
