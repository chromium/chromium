// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_content_dialog.css.js';
import {getHtml} from './app_content_dialog.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class AppContentDialogElement extends CrLitElement {
  static get is() {
    return 'app-management-app-content-dialog';
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
    this.addEventListener('keydown', e => this.trapFocus_(e as KeyboardEvent));
  }

  // The close button is the only tabbable element in the dialog, so focus
  // should stay on it.
  private trapFocus_(e: KeyboardEvent) {
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
    'app-management-app-content-dialog': AppContentDialogElement;
  }
}

customElements.define(AppContentDialogElement.is, AppContentDialogElement);
