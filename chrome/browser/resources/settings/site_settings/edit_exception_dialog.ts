// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-exception-dialog' is a component for editing a
 * site exception entry.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getTemplate} from './edit_exception_dialog.html.js';
import type {SiteException, SiteSettingsPrefsBrowserProxy} from './site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

export interface SettingsEditExceptionDialogElement {
  $: {
    dialog: CrDialogElement,
    actionButton: CrButtonElement,
  };
}

export class SettingsEditExceptionDialogElement extends PolymerElement {
  static get is() {
    return 'settings-edit-exception-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: {
        type: Object,
        observer: 'modelChanged_',
      },

      origin_: String,

      /**
       * The localized error message to display when the pattern is invalid.
       */
      errorMessage_: String,

      /**
       * Whether the current input is invalid.
       */
      invalid_: {
        type: Boolean,
        value: false,
      },
    };
  }

  model: SiteException;
  private origin_: string;
  private errorMessage_: string;
  private invalid_: boolean;
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.origin_ = this.model.origin;

    this.$.dialog.showModal();
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private onActionButtonClick_() {
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

  private validate_() {
    if (this.shadowRoot!.querySelector('cr-input')!.value.trim() === '') {
      this.invalid_ = true;
      return;
    }

    this.browserProxy_.isPatternValidForType(this.origin_, this.model.category)
        .then(({isValid, reason}) => {
          this.invalid_ = !isValid;
          this.errorMessage_ = reason || '';
        });
  }

  private modelChanged_() {
    if (!this.model) {
      this.$.dialog.cancel();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-edit-exception-dialog': SettingsEditExceptionDialogElement;
  }
}

customElements.define(
    SettingsEditExceptionDialogElement.is, SettingsEditExceptionDialogElement);
