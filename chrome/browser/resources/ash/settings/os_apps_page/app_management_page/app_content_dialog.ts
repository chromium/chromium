// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_cros_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_content_dialog.html.js';

const AppManagementAppContentDialogElementBase = I18nMixin(PolymerElement);

export class AppManagementAppContentDialogElement extends
    AppManagementAppContentDialogElementBase {
  static get is() {
    return 'app-management-app-content-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
    };
  }
  app: App;

  override ready(): void {
    super.ready();
    this.addEventListener('keydown', e => this.trapFocus_(e as KeyboardEvent));
  }

  // The close button is the only tabbable element in the dialog, so focus
  // should stay on it.
  private trapFocus_(e: KeyboardEvent): void {
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
    'app-management-app-content-dialog': AppManagementAppContentDialogElement;
  }
}

customElements.define(
    AppManagementAppContentDialogElement.is,
    AppManagementAppContentDialogElement);
