// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';
import {focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'diagnostic-page' displays critical device info to help technicians review
 * the current device state.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const DiagnosticPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class DiagnosticPage extends DiagnosticPageBase {
  static get is() {
    return 'diagnostic-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Set by shimless_rma.js.
      allButtonsDisabled: Boolean,
    };
  }

  constructor() {
    if (!loadTimeData.getBoolean('diagnosticPageEnabled')) {
      return;
    }

    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }
}

customElements.define(DiagnosticPage.is, DiagnosticPage);
