// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-import-confirmation-dialog' is a component
 * warning the user that importing a container overrides the existing container.
 * By clicking 'Continue', the user agrees to start the import.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GuestId} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

/** @polymer */
class SettingsCrostiniImportConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-import-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!GuestId} */
      importContainerId: {
        type: Object,
      },
    };
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onContinueTap_() {
    this.browserProxy_.importCrostiniContainer(this.importContainerId);
    this.$.dialog.close();
  }
}

customElements.define(
    SettingsCrostiniImportConfirmationDialogElement.is,
    SettingsCrostiniImportConfirmationDialogElement);
