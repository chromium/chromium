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
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getHtml} from './edit_exception_dialog.html.js';
import type {SiteException, SiteSettingsBrowserProxy} from './site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from './site_settings_browser_proxy.js';

export interface SettingsEditExceptionDialogElement {
  $: {
    dialog: CrDialogElement,
    actionButton: CrButtonElement,
    input: CrInputElement,
  };
}

export type EditExceptionDialogElement = SettingsEditExceptionDialogElement;

export class SettingsEditExceptionDialogElement extends CrLitElement {
  static get is() {
    return 'settings-edit-exception-dialog';
  }

  static override get styles() {
    return getSettingsSharedCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      model: {type: Object},
      origin_: {type: String},

      /**
       * The localized error message to display when the pattern is invalid.
       */
      errorMessage_: {type: String},

      /**
       * Whether the current input is invalid.
       */
      invalid_: {type: Boolean},
    };
  }

  accessor model: SiteException|null = null;
  protected accessor origin_: string = '';
  protected accessor errorMessage_: string = '';
  protected accessor invalid_: boolean = false;

  private browserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    assert(this.model);
    this.origin_ = this.model.origin;

    super.connectedCallback();

    this.$.dialog.showModal();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('model') && !this.model) {
      this.$.dialog.cancel();
    }
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onActionButtonClick_() {
    assert(this.model);
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

  protected onOriginInput_() {
    this.origin_ = this.$.input.value;
    if (this.origin_.trim() === '') {
      this.invalid_ = true;
      return;
    }

    assert(this.model);
    this.browserProxy_.isPatternValidForType(this.origin_, this.model.category)
        .then(({isValid, reason}) => {
          this.invalid_ = !isValid;
          this.errorMessage_ = reason || '';
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-edit-exception-dialog': SettingsEditExceptionDialogElement;
  }
}

customElements.define(
    SettingsEditExceptionDialogElement.is, SettingsEditExceptionDialogElement);
