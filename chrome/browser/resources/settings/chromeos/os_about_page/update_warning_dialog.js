// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-update-warning-dialog' is a component warning the
 * user about update over mobile data. By clicking 'Continue', the user
 * agrees to download update using mobile data.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo} from './about_page_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsUpdateWarningDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsUpdateWarningDialogElement extends
    SettingsUpdateWarningDialogElementBase {
  static get is() {
    return 'settings-update-warning-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!AboutPageUpdateInfo|undefined} */
      updateInfo: {
        type: Object,
        observer: 'updateInfoChanged_',
      },
    };
  }

  constructor() {
    super();

    /** @private {AboutPageBrowserProxy} */
    this.browserProxy_ = AboutPageBrowserProxyImpl.getInstance();
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
    if (!this.updateInfo || !this.updateInfo.version || !this.updateInfo.size){
      console.warn('ERROR: requestUpdateOverCellular arguments are undefined');
      return;
    }
    this.browserProxy_.requestUpdateOverCellular(
        /** @type {!string} */ (this.updateInfo.version),
        /** @type {!string} */ (this.updateInfo.size));
    this.$.dialog.close();
  }

  /** @private */
  updateInfoChanged_() {
    this.shadowRoot.querySelector('#update-warning-message').innerHTML =
        this.i18n(
            'aboutUpdateWarningMessage',
            // Convert bytes to megabytes
            Math.floor(Number(this.updateInfo.size) / (1024 * 1024)));
  }
}

customElements.define(
    SettingsUpdateWarningDialogElement.is, SettingsUpdateWarningDialogElement);
