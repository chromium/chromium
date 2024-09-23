// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Page in eSIM Cellular Setup flow shown if an eSIM profile requires a
 * confirmation code to install. This element contains an input for the user to
 * enter the confirmation code.
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {ESimProfileProperties} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './confirmation_code_page.html.js';

const ConfirmationCodePageElementBase = I18nMixin(PolymerElement);

export class ConfirmationCodePageElement extends
    ConfirmationCodePageElementBase {
  static get is() {
    return 'confirmation-code-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileProperties: Object,

      confirmationCode: {
        type: String,
        notify: true,
      },

      showError: Boolean,
    };
  }

  profileProperties?: ESimProfileProperties;
  confirmationCode: string;
  showError: boolean;

  private onKeyDown_(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
        bubbles: true,
        composed: true,
      }));
    }
    e.stopPropagation();
  }

  private getProfileName_(): string {
    if (!this.profileProperties) {
      return '';
    }
    return mojoString16ToString(this.profileProperties.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ConfirmationCodePageElement.is]: ConfirmationCodePageElement;
  }
}

customElements.define(
    ConfirmationCodePageElement.is, ConfirmationCodePageElement);
