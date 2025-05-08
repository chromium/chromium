// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {getHtml} from './base_dialog_app.html.js';
import {BaseDialogBrowserProxy} from './base_dialog_browser_proxy.js';

export class BaseDialogApp extends CrLitElement {
  static get is() {
    return 'base-dialog-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  private handler_: BaseDialogPageHandlerInterface;

  override firstUpdated() {
    this.handler_ = BaseDialogBrowserProxy.getInstance().getHandler();
    this.resizeAndShowNativeDialog();
  }

  private resizeAndShowNativeDialog(): Promise<void> {
    return new Promise(async resolve => {
      // Prefer using |document.body.offsetHeight| instead of
      // |document.body.scrollHeight| as it returns the correct height
      // of the page even when the page zoom in Chrome is different
      // than 100%.
      await this.handler_.resizeDialog(document.body.offsetHeight);
      // After the layout is adjusted to fit into the dialog, show
      // the native dialog.
      this.handler_.showDialog();
      resolve();
    });
  }

  // TODO(crbug.com/398005782): Temporary callback to close the dialog for
  // manual testing.
  protected onCloseButton_() {
    this.handler_.closeDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'base-dialog-app': BaseDialogApp;
  }
}

customElements.define(BaseDialogApp.is, BaseDialogApp);
