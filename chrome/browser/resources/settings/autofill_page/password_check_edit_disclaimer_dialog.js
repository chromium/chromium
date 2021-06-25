// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsPasswordEditDisclaimerDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsPasswordEditDisclaimerDialogElement extends
    SettingsPasswordEditDisclaimerDialogElementBase {
  static get is() {
    return 'settings-password-edit-disclaimer-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The website origin that is being displayed.
       */
      origin: String,
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @private */
  onEditClick_() {
    this.dispatchEvent(new CustomEvent(
        'edit-password-click', {bubbles: true, composed: true}));
    this.$.dialog.close();
  }

  /** @private */
  onCancel_() {
    this.$.dialog.close();
  }

  /**
   * @return {string}
   * @private
   */
  getDisclaimerTitle_() {
    return this.i18n('editDisclaimerTitle', this.origin);
  }
}

customElements.define(
    SettingsPasswordEditDisclaimerDialogElement.is,
    SettingsPasswordEditDisclaimerDialogElement);
