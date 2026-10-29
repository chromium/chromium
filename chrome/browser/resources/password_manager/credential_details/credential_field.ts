// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {getCss as getCrIconsCss} from 'chrome://resources/cr_elements/cr_icons_lit.css.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {getCss as getCrInputStyleCss} from 'chrome://resources/cr_elements/cr_input/cr_input_style_lit.css.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {getCss as getSharedStyleCss} from '../shared_style_lit.css.js';

import {getCss as getCredentialDetailsCardCss} from './credential_details_card_lit.css.js';
import {getHtml} from './credential_field.html.js';

export interface CredentialFieldElement {
  $: {
    inputValue: CrInputElement,
    copyButton: CrIconButtonElement,
  };
}

// An element that represents a credential field with a 'copy' button.
export class CredentialFieldElement extends CrLitElement {
  static get is() {
    return 'credential-field';
  }

  static override get styles() {
    return [
      getCrIconsCss(),
      getCrInputStyleCss(),
      getCrSharedStyleCss(),
      getSharedStyleCss(),
      getCredentialDetailsCardCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The label on the actual input element. Required.
       */
      label: {type: String},

      /**
       * The label on the copy button. Required.
       */
      copyButtonLabel: {type: String},

      /**
       * Text that appears on the toast when clicking the copy button.
       * Required.
       */
      valueCopiedToastLabel: {type: String},

      /**
       * Field value.
       */
      value: {type: String},

      /*
       * Placeholder when the value is empty.
       */
      placeholder: {type: String},

      /**
       * If set, clicking the copy button will record this password view
       * interaction.
       */
      interactionId: {type: Number},
    };
  }

  accessor label: string = '';
  accessor copyButtonLabel: string = '';
  accessor valueCopiedToastLabel: string = '';
  accessor value: string = '';
  accessor placeholder: string = '';
  accessor interactionId: PasswordViewPageInteractions|undefined;

  override connectedCallback() {
    super.connectedCallback();
    assert(this.label);
    assert(this.copyButtonLabel);
    assert(this.valueCopiedToastLabel);
  }

  protected onCopyValueClick_() {
    navigator.clipboard.writeText(this.value).catch(() => {});
    this.showToast_();
    PasswordManagerImpl.getInstance().extendAuthValidity();
    if (this.interactionId !== undefined) {
      PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
          this.interactionId);
    }
  }

  private showToast_() {
    this.fire('value-copied', {toastMessage: this.valueCopiedToastLabel});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'credential-field': CredentialFieldElement;
  }
}

customElements.define(CredentialFieldElement.is, CredentialFieldElement);
