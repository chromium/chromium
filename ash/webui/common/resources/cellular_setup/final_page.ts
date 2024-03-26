// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Final page in Cellular Setup flow, which either displays a success or error
 * message depending on the outcome of the flow. This element contains an image
 * asset and description that indicates that the setup flow has completed.
 */
import './base_page.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {getTemplate} from './final_page.html.js';

const FinalPageElementBase = I18nMixin(PolymerElement);

export class FinalPageElement extends FinalPageElementBase {
  static get is() {
    return 'final-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      /**
       * Whether error state should be shown.
       */
      showError: Boolean,

      message: String,

      errorMessage: String,
    };
  }

  delegate: CellularSetupDelegate;
  showError: boolean;
  message: string;
  errorMessage: string;

  private getTitle_(showError: boolean): string|null {
    if (this.delegate.shouldShowPageTitle()) {
      return showError ? this.i18n('finalPageErrorTitle') :
                         this.i18n('finalPageTitle');
    }
    return null;
  }

  private getMessage_(showError: boolean): string {
    return showError ? this.errorMessage : this.message;
  }

  private getPageBodyClass_(showError: boolean): string {
    return showError ? 'error' : '';
  }

  private getJellyIllustrationName_(showError: boolean): string {
    return showError ? 'cellular-setup-illo:error' :
                       'cellular-setup-illo:final-page-success';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FinalPageElement.is]: FinalPageElement;
  }
}

customElements.define(FinalPageElement.is, FinalPageElement);
