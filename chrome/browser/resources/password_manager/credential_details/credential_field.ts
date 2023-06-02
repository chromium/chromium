// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';

import {getTemplate} from './credential_field.html.js';

export interface CredentialFieldElement {
  $: {
    inputValue: CrInputElement,
    copyButton: CrIconButtonElement,
    toast: CrToastElement,
  };
}

// An element that represents a credential field with a 'copy' button.
export class CredentialFieldElement extends PolymerElement {
  static get is() {
    return 'credential-field';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The label on the actual input element. Required.
       */
      label: String,

      /**
       * The label on the copy button. Required.
       */
      copyButtonLabel: String,

      /**
       * Text that appears on the toast when clicking the copy button.
       * Required.
       */
      valueCopiedToastLabel: String,

      /**
       * Field value.
       */
      value: String,

      /**
       * If set, clicking the copy button will record this password view
       * interaction.
       */
      interactionId: PasswordViewPageInteractions,
    };
  }

  label: string;
  copyButtonLabel: string;
  valueCopiedToastLabel: string;
  value: string;
  interactionId: PasswordViewPageInteractions;

  override connectedCallback() {
    super.connectedCallback();
    assert(this.label);
    assert(this.copyButtonLabel);
    assert(this.valueCopiedToastLabel);
  }

  private onCopyValueClick_() {
    navigator.clipboard.writeText(this.value).catch(() => {});
    this.$.toast.show();
    PasswordManagerImpl.getInstance().extendAuthValidity();
    if (this.interactionId) {
      PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
          this.interactionId);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'credential-field': CredentialFieldElement;
  }
}

customElements.define(CredentialFieldElement.is, CredentialFieldElement);
