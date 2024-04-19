// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-site-dialog' provides a dialog to add exceptions for a given Content
 * Settings category.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './add_site_dialog.html.js';
import {ContentSetting, ContentSettingsTypes, CookiesExceptionType, SITE_EXCEPTION_WILDCARD} from './constants.js';
import type {SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface AddSiteDialogElement {
  $: {
    add: CrButtonElement,
    dialog: CrDialogElement,
    incognito: CrCheckboxElement,
    site: CrInputElement,
  };
}

const AddSiteDialogElementBase = SiteSettingsMixin(PolymerElement) as unknown as
    {new (): PolymerElement & SiteSettingsMixinInterface};

export class AddSiteDialogElement extends AddSiteDialogElementBase {
  static get is() {
    return 'add-site-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether this is about an Allow, Block, SessionOnly, or other.
       */
      contentSetting: String,

      hasIncognito: {
        type: Boolean,
        observer: 'hasIncognitoChanged_',
      },

      /**
       * Controls what kind of patterns the created cookies exception will have
       * (based on the CookiesExceptionType):
       * - THIRD_PARTY: Exception that will have primary pattern as wildcard
       * (third-party cookie exceptions).
       * - SITE_DATA: Exception that will have secondary pattern as wildcard
       * (regular exceptions).
       * - COMBINED: Support both pattern types and have a checkbox to control
       * the mode.
       */
      cookiesExceptionType: String,

      /**
       * The site to add an exception for.
       */
      site_: String,

      /**
       * The error message to display when the pattern is invalid.
       */
      errorMessage_: String,
    };
  }

  contentSetting: ContentSetting;
  hasIncognito: boolean;
  private site_: string;
  private errorMessage_: string;
  cookiesExceptionType: CookiesExceptionType;

  override connectedCallback() {
    super.connectedCallback();

    assert(this.category);
    assert(this.contentSetting);
    assert(typeof this.hasIncognito !== 'undefined');

    this.$.dialog.showModal();
  }

  /**
   * Validates that the pattern entered is valid.
   */
  private validate_() {
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.$.site.value.trim() === '') {
      this.$.site.invalid = false;
      this.$.add.disabled = true;
      return;
    }

    this.browserProxy.isPatternValidForType(this.site_, this.category)
        .then(({isValid, reason}) => {
          this.$.site.invalid = !isValid;
          this.$.add.disabled = !isValid;
          this.errorMessage_ = reason || '';
        });
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   */
  private onSubmit_() {
    assert(!this.$.add.disabled);
    let primaryPattern = this.site_;
    let secondaryPattern = SITE_EXCEPTION_WILDCARD;

    if (this.cookiesExceptionType === CookiesExceptionType.THIRD_PARTY ||
        this.category === ContentSettingsTypes.TRACKING_PROTECTION) {
      primaryPattern = SITE_EXCEPTION_WILDCARD;
      secondaryPattern = this.site_;
    }

    this.browserProxy.setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, this.category, this.contentSetting,
        this.$.incognito.checked);

    this.$.dialog.close();
  }

  private showIncognitoSessionOnly_() {
    return this.hasIncognito && !loadTimeData.getBoolean('isGuest') &&
        this.contentSetting !== ContentSetting.SESSION_ONLY;
  }

  private hasIncognitoChanged_() {
    if (!this.hasIncognito) {
      this.$.incognito.checked = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-site-dialog': AddSiteDialogElement;
  }
}

customElements.define(AddSiteDialogElement.is, AddSiteDialogElement);
