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

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './add_site_dialog.css.js';
import {getHtml} from './add_site_dialog.html.js';
import {ContentSetting, CookiesExceptionType, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface AddSiteDialogElement {
  $: {
    add: CrButtonElement,
    dialog: CrDialogElement,
    incognito: CrCheckboxElement,
    site: CrInputElement,
  };
}

const AddSiteDialogElementBase = SiteSettingsMixinLit(CrLitElement);

export class AddSiteDialogElement extends AddSiteDialogElementBase {
  static get is() {
    return 'add-site-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether this is about an Allow, Block, SessionOnly, or other.
       */
      contentSetting: {type: String},

      hasIncognito: {type: Boolean},

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
      cookiesExceptionType: {type: String},

      /**
       * The site to add an exception for.
       */
      site_: {type: String},

      /**
       * The error message to display when the pattern is invalid.
       */
      errorMessage_: {type: String},
    };
  }

  accessor contentSetting: ContentSetting = ContentSetting.DEFAULT;
  accessor hasIncognito: boolean = false;
  accessor cookiesExceptionType: CookiesExceptionType|undefined = undefined;
  protected accessor site_: string = '';
  protected accessor errorMessage_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    queueMicrotask(() => {
      assert(this.category);
      assert(this.contentSetting);
      assert(typeof this.hasIncognito !== 'undefined');
    });

    this.$.dialog.showModal();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('hasIncognito')) {
      if (!this.hasIncognito) {
        this.$.incognito.checked = false;
      }
    }
  }

  /**
   * Validates that the pattern entered is valid.
   */
  protected onInput_() {
    this.site_ = this.$.site.value;
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.site_.trim() === '') {
      this.$.site.invalid = false;
      this.$.add.disabled = true;
      return;
    }

    assert(this.category);
    this.browserProxy.isPatternValidForType(this.site_, this.category)
        .then(({isValid, reason}) => {
          this.$.site.invalid = !isValid;
          this.$.add.disabled = !isValid;
          this.errorMessage_ = reason || '';
        });
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   */
  protected onSubmitClick_() {
    assert(!this.$.add.disabled);
    let primaryPattern = this.site_;
    let secondaryPattern = SITE_EXCEPTION_WILDCARD;

    if (this.cookiesExceptionType === CookiesExceptionType.THIRD_PARTY) {
      primaryPattern = SITE_EXCEPTION_WILDCARD;
      secondaryPattern = this.site_;
    }

    assert(this.category);
    this.browserProxy.setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, this.category, this.contentSetting,
        this.$.incognito.checked);

    this.$.dialog.close();
  }

  protected showIncognitoSessionOnly_(): boolean {
    return this.hasIncognito && !loadTimeData.getBoolean('isGuest') &&
        this.contentSetting !== ContentSetting.SESSION_ONLY;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-site-dialog': AddSiteDialogElement;
  }
}

customElements.define(AddSiteDialogElement.is, AddSiteDialogElement);
