// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-exception-dialog' is a component for editing a
 * site exception entry.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SITE_EXCEPTION_WILDCARD} from './constants.js';
import {SiteException, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';


/** @polymer */
export class SettingsEditExceptionDialogElement extends PolymerElement {
  static get is() {
    return 'settings-edit-exception-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!SiteException}
       */
      model: {
        type: Object,
        observer: 'modelChanged_',
      },

      /** @private */
      origin_: String,

      /**
       * The localized error message to display when the pattern is invalid.
       * @private
       */
      errorMessage_: String,

      /**
       * Whether the current input is invalid.
       * @private
       */
      invalid_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {!SiteSettingsPrefsBrowserProxy} */
    this.browserProxy_ = SiteSettingsPrefsBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.origin_ = this.model.origin;

    this.$.dialog.showModal();
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onActionButtonTap_() {
    if (this.model.origin !== this.origin_) {
      // The way to "edit" an exception is to remove it and and a new one.
      this.browserProxy_.resetCategoryPermissionForPattern(
          this.model.origin, this.model.embeddingOrigin, this.model.category,
          this.model.incognito);

      this.browserProxy_.setCategoryPermissionForPattern(
          this.origin_, SITE_EXCEPTION_WILDCARD, this.model.category,
          this.model.setting, this.model.incognito);
    }

    this.$.dialog.close();
  }

  /** @private */
  validate_() {
    if (this.shadowRoot.querySelector('cr-input').value.trim() === '') {
      this.invalid_ = true;
      return;
    }

    this.browserProxy_.isPatternValidForType(this.origin_, this.model.category)
        .then(({isValid, reason}) => {
          this.invalid_ = !isValid;
          this.errorMessage_ = reason || '';
        });
  }

  /** @private */
  modelChanged_() {
    if (!this.model) {
      this.$.dialog.cancel();
    }
  }
}

customElements.define(
    SettingsEditExceptionDialogElement.is, SettingsEditExceptionDialogElement);
