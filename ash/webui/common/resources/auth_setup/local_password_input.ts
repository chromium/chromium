// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './local_password_input.html.js';

const LocalPasswordInputElementBase = I18nMixin(PolymerElement);

export interface LocalPasswordInputElement {
  $: {
    passwordInput: CrInputElement,
    confirmPasswordInput: CrInputElement,
  };
}

/**
 * @fileoverview 'local-password-input' is a component that consists of two
 * input fields
 *
 * It is used for allowing the user to set up a local password. Validation of
 * password complexity is done on browser side. Therefore, clients of this
 * element are expected to implement their own validation logic. The content of
 * the two fields must match before submission and this is enforced by the
 * element itself.
 */
export class LocalPasswordInputElement extends LocalPasswordInputElementBase {
  static get is(): string {
    return 'local-password-input' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): object {
    return {
      // Localized strings must be passed down from owning component.
      passwordInputPlaceholderText: {
        type: String,
        value: '',
      },

      confirmPasswordInputPlaceholderText: {
        type: String,
        value: '',
      },

      passwordMismatchErrorMessage: {
        type: String,
        value: '',
      },
    };
  }

  addSubmitListener(element: Element, callback: () => void): void {
    element.addEventListener('keydown', (e: Event) => {
      const keyboardEvent = e as KeyboardEvent;
      if (keyboardEvent.key === 'Enter') {
        callback();
      }
    });
  }

  override ready(): void {
    super.ready();
    this.$.passwordInput.focus();
    this.addSubmitListener(this.$.passwordInput, () => {
      this.submit();
    });
    this.addSubmitListener(this.$.confirmPasswordInput, () => {
      this.submit();
    });
  }

  private submit(): void {
    this.dispatchEvent(
        new CustomEvent('submit', {bubbles: true, composed: true}));
  }
}

customElements.define(LocalPasswordInputElement.is, LocalPasswordInputElement);
declare global {
  interface HTMLElementTagNameMap {
    'local-password-input': LocalPasswordInputElement;
  }
}
