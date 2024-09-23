// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays SeaPen errors.
 */

import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {MantaStatusCode} from './sea_pen.mojom-webui.js';
import {getTemplate} from './sea_pen_error_element.html.js';

const SeaPenErrorElementBase = I18nMixin(PolymerElement);

export class SeaPenErrorElement extends SeaPenErrorElementBase {
  static get is() {
    return 'sea-pen-error';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      thumbnailResponseStatusCode: {
        type: Object,
      },

      errorMessage_: {
        type: String,
        computed: 'getErrorMessage_(thumbnailResponseStatusCode)',
      },

      errorIllo_: {
        type: String,
        computed: 'getErrorIllo_(thumbnailResponseStatusCode)',
      },
    };
  }

  thumbnailResponseStatusCode: MantaStatusCode;
  private errorMessage_: string;
  private errorIllo_: string;

  private getErrorMessage_(statusCode: MantaStatusCode): string {
    switch (statusCode) {
      case MantaStatusCode.kNoInternetConnection:
        return isSeaPenTextInputEnabled() ?
            this.i18n('seaPenFreeformErrorNoInternet') :
            this.i18n('seaPenErrorNoInternet');
      case MantaStatusCode.kPerUserQuotaExceeded:
      case MantaStatusCode.kResourceExhausted:
        return this.i18n('seaPenErrorResourceExhausted');
    }

    if (isSeaPenTextInputEnabled()) {
      switch (statusCode) {
        case MantaStatusCode.kUnsupportedLanguage:
          return this.i18n('seaPenFreeformErrorUnsupportedLanguage');
        case MantaStatusCode.kBlockedOutputs:
          return this.i18n('seaPenFreeformErrorBlockedOutputs');
      }
    }
    return this.i18n('seaPenErrorGeneric');
  }

  private getErrorIllo_(statusCode: MantaStatusCode): string {
    switch (statusCode) {
      case MantaStatusCode.kNoInternetConnection:
        return 'personalization-shared-illo:network_error';
      case MantaStatusCode.kPerUserQuotaExceeded:
      case MantaStatusCode.kResourceExhausted:
        return 'personalization-shared-illo:resource_error';
      default:
        return 'personalization-shared-illo:generic_error';
    }
  }
}

customElements.define(SeaPenErrorElement.is, SeaPenErrorElement);
