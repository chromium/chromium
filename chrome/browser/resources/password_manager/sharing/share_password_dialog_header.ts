// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './metrics_utils.js';
import '../shared_style.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
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

  static get properties() {
    return {
      isError: {
        type: Boolean,
        value: false,
      },
    };
  }

  isError: boolean;

  private onHelpClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.DIALOG_HEADER_HELP_ICON_BUTTON_CLICKED);

    if (this.isError) {
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('passwordSharingTroubleshootURL'));
      return;
    }
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('passwordSharingLearnMoreURL'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-dialog-header': SharePasswordDialogHeaderElement;
  }
}

customElements.define(
    SharePasswordDialogHeaderElement.is, SharePasswordDialogHeaderElement);
