// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Shimless3pDiagnosticsAppInfo, ShimlessRmaServiceInterface, Show3pDiagnosticsAppResult} from './shimless_rma_types.js';
import {disableAllButtons, enableAllButtons} from './shimless_rma_util.js';

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
    return {
      /** @private */
      hasPendingLaunch_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      providerName_: {
        type: String,
        value: '',
      },

      /** @protected */
      errorTitle_: {
        type: String,
        value: '',
      },

      /** @protected */
      errorMessage_: {
        type: String,
        value: '',
      },
    };
  }

  /** @override */
  constructor() {
    super();

    if (!loadTimeData.getBoolean('3pDiagnosticsEnabled')) {
      return;
    }

    /** @private {!ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    this.shimlessRmaService_.get3pDiagnosticsProvider().then(
        /**@type function({provider: ?string})*/ (({provider}) => {
          this.providerName_ = provider;
        }));
  }

  /**
   * Ends the launch process and enables all buttons.
   * @private
   */
  completeLaunch_() {
    this.hasPendingLaunch_ = false;
    enableAllButtons(this);
  }

  /**
   * Shows the error dialog with specific title and message.
   * @param {string} titleId
   * @param {string} messageId
   * @private
   */
  showError_(titleId, messageId) {
    this.errorTitle_ = this.i18n(titleId, this.providerName_);
    this.errorMessage_ = this.i18n(messageId);
    this.root.querySelector('#shimless3pDiagErrorDialog').showModal();
  }

  /**
   * Shows the dialog to ask whether users want to install an installable app
   * from external storage or not.
   * @param {mojoBase.mojom.FilePath} appPath
   * @private
   */
  showFindInstallableDialog_(appPath) {
    // TODO(chungsheng): Implement this.
  }

  /**
   * Shows the 3p diagnostics app. The app dialog will be shown in another
   * webview outside of this webui. If no app is installed or fails to load
   * installed app, shows the error.
   * @private
   */
  show3pDiagnosticsApp_() {
    this.shimlessRmaService_.show3pDiagnosticsApp().then((result) => {
      switch (result.result) {
        case Show3pDiagnosticsAppResult.kOk:
          this.completeLaunch_();
          return;
        case Show3pDiagnosticsAppResult.kAppNotInstalled:
          this.showError_(
              '3pNotInstalledDialogTitle', '3pCheckWithOemDialogMessage');
          return;
        case Show3pDiagnosticsAppResult.kFailedToLoad:
          this.showError_(
              '3pFailedToLoadDialogTitle', '3pFailedToLoadDialogMessage');
          return;
      }
      assertNotReached();
    });
  }

  /**
   * Handles cancel event or back button of error dialog. Close the dialog and
   * ends the launch process.
   * @protected
   */
  onErrorDialogCancelOrBackButtonClicked_() {
    this.shadowRoot.querySelector('#shimless3pDiagErrorDialog').close();
    this.completeLaunch_();
  }

  /**
   * Launch the 3p diagnostics app. This will ask if users want to install the
   * app from external storage if app files exist, or launch the installed app.
   * @public
   */
  launch3pDiagnostics() {
    if (!loadTimeData.getBoolean('3pDiagnosticsEnabled') ||
        this.hasPendingLaunch_) {
      return;
    }

    // If there is no provider or provider is not yet fetched, don't show
    // any UI action and just return.
    if (!this.providerName_) {
      return;
    }

    this.hasPendingLaunch_ = true;
    disableAllButtons(this, /*showBusyStateOverlay=*/ true);

    this.shimlessRmaService_.getInstallable3pDiagnosticsAppPath().then(
        (result) => {
          if (result.appPath) {
            this.showFindInstallableDialog_(result.appPath);
          } else {
            this.show3pDiagnosticsApp_();
          }
        });
  }
}

customElements.define(Shimless3pDiagnostics.is, Shimless3pDiagnostics);
