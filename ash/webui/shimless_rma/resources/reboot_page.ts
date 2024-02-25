// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';
import './shimless_rma_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getTemplate} from './reboot_page.html.js';
import {RmadErrorCode} from './shimless_rma.mojom-webui.js';
import {focusPageTitle} from './shimless_rma_util.js';

// The displayed value for how many seconds you wait before the reboot or shut
// down.
const DELAY_DURATION = '3';

/**
 * @fileoverview
 * 'reboot-page' is displayed while waiting for a reboot.
 */

const RebootPageBase = I18nMixin(PolymerElement);

export class RebootPage extends RebootPageBase {
  static get is() {
    return 'reboot-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.ts.
       */
      errorCode: {
        type: Object,
      },
    };
  }

  errorCode: RmadErrorCode;

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected getPageTitle(): string {
    return this.errorCode === RmadErrorCode.kExpectReboot ?
        this.i18n('rebootPageTitle') :
        this.i18n('shutdownPageTitle');
  }

  protected getPageInstructions(): string {
    return this.errorCode === RmadErrorCode.kExpectReboot ?
        this.i18n('rebootPageMessage', DELAY_DURATION) :
        this.i18n('shutdownPageMessage', DELAY_DURATION);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RebootPage.is]: RebootPage;
  }
}

customElements.define(RebootPage.is, RebootPage);
