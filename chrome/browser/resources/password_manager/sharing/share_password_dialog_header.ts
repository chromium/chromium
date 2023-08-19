// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../shared_style.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_dialog_header.html.js';

export interface SharePasswordDialogHeaderElement {
  $: {
    helpButton: HTMLElement,
  };
}

export class SharePasswordDialogHeaderElement extends PolymerElement {
  static get is() {
    return 'share-password-dialog-header';
  }

  static get template() {
    return getTemplate();
  }

  private onHelpClick_() {
    // TODO(crbug/1445526): Update learn more url to be more specific.
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('passwordManagerLearnMoreURL'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-dialog-header': SharePasswordDialogHeaderElement;
  }
}

customElements.define(
    SharePasswordDialogHeaderElement.is, SharePasswordDialogHeaderElement);
