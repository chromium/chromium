// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'shimless-3p-diagnostics' manages dialogs to install and show 3p diagnostics
 * app.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const Shimless3pDiagnosticsBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class Shimless3pDiagnostics extends Shimless3pDiagnosticsBase {
  static get is() {
    return 'shimless-3p-diagnostics';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  /** @override */
  constructor() {
    super();

    if (!loadTimeData.getBoolean('3pDiagnosticsEnabled')) {
      return;
    }

    /** @private {!ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /**
   * Launch the 3p diagnostics app. This will ask if users want to install the
   * app from external storage if app files exist, or launch the installed app.
   * @public
   */
  launch3pDiagnostics() {
    // TODO(chungsheng): Implement the logic.
  }
}

customElements.define(Shimless3pDiagnostics.is, Shimless3pDiagnostics);
