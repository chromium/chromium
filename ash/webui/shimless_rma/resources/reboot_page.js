// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RmadErrorCode} from './shimless_rma_types.js';
import {focusPageTitle} from './shimless_rma_util.js';

// The displayed value for how many seconds you wait before the reboot or shut
// down.
const DELAY_DURATION = '3';

/**
 * @fileoverview
 * 'reboot-page' is displayed while waiting for a reboot.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const RebootPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class RebootPage extends RebootPageBase {
  static get is() {
    return 'reboot-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {RmadErrorCode}
       */
      errorCode: {
        type: Object,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @return {string}
   * @protected
   */
  getPageTitle_() {
    return this.errorCode === RmadErrorCode.kExpectReboot ?
        this.i18n('rebootPageTitle') :
        this.i18n('shutdownPageTitle');
  }

  /**
   * @return {string}
   * @protected
   */
  getPageInstructions_() {
    return this.errorCode === RmadErrorCode.kExpectReboot ?
        this.i18n('rebootPageMessage', DELAY_DURATION) :
        this.i18n('shutdownPageMessage', DELAY_DURATION);
  }
}

customElements.define(RebootPage.is, RebootPage);
