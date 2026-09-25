// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'security-keys-subpage' is a settings subpage
 * containing operations on security keys.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_page/settings_subpage.js';
import './security_keys_credential_management_dialog.js';
import './security_keys_bio_enroll_dialog.js';
import './security_keys_set_pin_dialog.js';
import './security_keys_reset_dialog.js';

import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../../settings_shared_lit.css.js';

import {getHtml} from './security_keys_subpage.html.js';

export interface SecurityKeysSubpageElement {
  $: {
    setPINButton: HTMLElement,
    resetButton: HTMLElement,
  };
}

const SecurityKeysSubpageElementBase = SettingsViewMixinLit(CrLitElement);

export class SecurityKeysSubpageElement extends SecurityKeysSubpageElementBase {
  static get is() {
    return 'security-keys-subpage';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableBioEnrollment_: {type: Boolean},
      showSetPINDialog_: {type: Boolean},
      showCredentialManagementDialog_: {type: Boolean},
      showResetDialog_: {type: Boolean},
      showBioEnrollDialog_: {type: Boolean},
    };
  }

  protected accessor enableBioEnrollment_: boolean =
      loadTimeData.getBoolean('enableSecurityKeysBioEnrollment');
  protected accessor showSetPINDialog_: boolean = false;
  protected accessor showCredentialManagementDialog_: boolean = false;
  protected accessor showResetDialog_: boolean = false;
  protected accessor showBioEnrollDialog_: boolean = false;

  protected onSetPinClick_() {
    this.showSetPINDialog_ = true;
  }

  protected onSetPinDialogClose_() {
    this.showSetPINDialog_ = false;
    focusWithoutInk(this.$.setPINButton);
  }

  protected onCredentialManagementClick_() {
    this.showCredentialManagementDialog_ = true;
  }

  protected onCredentialManagementSetPin_() {
    this.onSetPinClick_();
  }

  protected onCredentialManagementDialogClose_() {
    this.showCredentialManagementDialog_ = false;
    const toFocus = this.shadowRoot.querySelector<HTMLElement>(
        '#credentialManagementButton');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  protected onResetClick_() {
    this.showResetDialog_ = true;
  }

  protected onResetDialogClose_() {
    this.showResetDialog_ = false;
    focusWithoutInk(this.$.resetButton);
  }

  protected onBioEnrollClick_() {
    this.showBioEnrollDialog_ = true;
  }

  protected onBioEnrollSetPin_() {
    this.onSetPinClick_();
  }

  protected onBioEnrollDialogClose_() {
    this.showBioEnrollDialog_ = false;
    const toFocus =
        this.shadowRoot.querySelector<HTMLElement>('#bioEnrollButton');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'security-keys-subpage': SecurityKeysSubpageElement;
  }
}

customElements.define(
    SecurityKeysSubpageElement.is, SecurityKeysSubpageElement);
