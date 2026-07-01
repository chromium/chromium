// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import '/strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {SmbBrowserProxy, SmbBrowserProxyImpl} from 'chrome://resources/ash/common/smb_shares/smb_browser_proxy.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './smb_credentials_dialog.html.js';

/**
 * @fileoverview
 * 'smb-credentials-dialog' is used to update the credentials for a mounted
 * smb share.
 */

interface SmbCredentialsDialogElement {
  $: {
    dialog: CrDialogElement,
    action: CrButtonElement,
  };
}

const SmbCredentialsDialogElementBase = I18nMixin(PolymerElement);

class SmbCredentialsDialogElement extends SmbCredentialsDialogElementBase {
  static get is() {
    return 'smb-credentials-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sharePath_: String,
      username_: String,
      password_: String,

    };
  }

  declare private sharePath_: string;
  declare private username_: string;
  declare private password_: string;
  private browserProxy_: SmbBrowserProxy = SmbBrowserProxyImpl.getInstance();

  constructor() {
    super();

    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    const dialogArgs = chrome.getVariableValue('dialogArguments');
    assert(dialogArgs);
    const args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.path);
    this.sharePath_ = args.path;

    this.$.dialog.showModal();
  }

  private onCancelButtonClick_() {
    chrome.send('dialogClose');
  }

  private onSaveButtonClick_() {
    this.browserProxy_.updateCredentials(this.username_, this.password_);
    chrome.send('dialogClose');
  }
}

customElements.define(
    SmbCredentialsDialogElement.is, SmbCredentialsDialogElement);
