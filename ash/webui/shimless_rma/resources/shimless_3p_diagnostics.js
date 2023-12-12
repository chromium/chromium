// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './shimless_3p_diagnostics.html.js';
import {Shimless3pDiagnosticsAppInfo, ShimlessRmaServiceInterface, Show3pDiagnosticsAppResult} from './shimless_rma.mojom-webui.js';
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
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private */
      hasPendingLaunch: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      providerName: {
        type: String,
        value: '',
      },

      /** @protected */
      installableAppPath: {
        type: String,
        value: '',
      },

      /** @protected {Shimless3pDiagnosticsAppInfo} */
      appInfo: {
        type: Object,
        value: null,
      },

      /** @protected */
      errorTitle: {
        type: String,
        value: '',
      },

      /** @protected */
      errorMessage: {
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
    this.shimlessRmaService = getShimlessRmaService();
    this.shimlessRmaService.get3pDiagnosticsProvider().then(
        /**@type function({provider: ?string})*/ (({provider}) => {
          this.providerName = provider;
        }));
  }

  /**
   * Ends the launch process and enables all buttons.
   * @private
   */
  completeLaunch() {
    this.hasPendingLaunch = false;
    enableAllButtons(this);
  }

  /**
   * Shows the error dialog with specific title and message.
   * @param {string} titleId
   * @param {string} messageId
   * @private
   */
  showError(titleId, messageId) {
    this.errorTitle = this.i18n(titleId, this.providerName);
    this.errorMessage = this.i18n(messageId);
    this.root.querySelector('#shimless3pDiagErrorDialog').showModal();
  }

  /**
   * Shows the dialog to ask whether users want to install an installable app
   * from external storage or not.
   * @param {mojoBase.mojom.FilePath} appPath
   * @private
   */
  showFindInstallableDialog(appPath) {
    this.installableAppPath = appPath.path;
    this.root.querySelector('#shimless3pDiagFindInstallableDialog').showModal();
  }

  /**
   * Completes the last installation by replying the user approval result. If
   * the installation is approved, then show the app. Otherwise, just finish the
   * launch process.
   * @param {boolean} isApproved
   * @private
   */
  completeLastInstall(isApproved) {
    this.shimlessRmaService.completeLast3pDiagnosticsInstallation(isApproved)
        .then(() => {
          isApproved ? this.show3pDiagnosticsApp() : this.completeLaunch();
        });
  }

  /**
   * Shows the permission review dialog for users to review the app permissions.
   * If there is no permission requested, the dialog won't be shown and the app
   * is approved.
   * @param {Shimless3pDiagnosticsAppInfo} appInfo
   * @private
   */
  showPermissionReviewDialogOrCompleteLastInstall(appInfo) {
    if (!appInfo.permissionMessage) {
      this.completeLastInstall(true);
      return;
    }

    this.appInfo = appInfo;
    this.root.querySelector('#shimless3pDiagReviewPermissionDialog')
        .showModal();
  }

  /**
   * Shows the 3p diagnostics app. The app dialog will be shown in another
   * webview outside of this webui. If no app is installed or fails to load
   * installed app, shows the error.
   * @private
   */
  show3pDiagnosticsApp() {
    this.shimlessRmaService.show3pDiagnosticsApp().then((result) => {
      switch (result.result) {
        case Show3pDiagnosticsAppResult.kOk:
          this.completeLaunch();
          return;
        case Show3pDiagnosticsAppResult.kAppNotInstalled:
          this.showError(
              '3pNotInstalledDialogTitle', '3pCheckWithOemDialogMessage');
          return;
        case Show3pDiagnosticsAppResult.kFailedToLoad:
          this.showError(
              '3pFailedToLoadDialogTitle', '3pFailedToLoadDialogMessage');
          return;
      }
      assertNotReached();
    });
  }

  /**
   * Handles cancel event or skip button of installable dialog. Skips the
   * install process and tries to load the installed app.
   * @protected
   */
  onCancelOrSkipInstallButtonClicked() {
    this.shadowRoot.querySelector('#shimless3pDiagFindInstallableDialog')
        .close();
    this.show3pDiagnosticsApp();
  }

  /**
   * Handles install button of installable dialog. Installs the installable app.
   * @protected
   */
  onInstallButtonClicked() {
    this.shadowRoot.querySelector('#shimless3pDiagFindInstallableDialog')
        .close();
    this.shimlessRmaService.installLastFound3pDiagnosticsApp().then(
        (result) => {
          result.appInfo ? this.showPermissionReviewDialogOrCompleteLastInstall(
                               result.appInfo) :
                           this.showError(
                               '3pFailedToInstallDialogTitle',
                               '3pCheckWithOemDialogMessage');
        });
  }

  /**
   * Handles cancel event or cancel button of permission review dialog. Cancels
   * the installation and ends the launch process.
   * @protected
   */
  onCancelOrCancelInstallButtonClicked() {
    this.shadowRoot.querySelector('#shimless3pDiagReviewPermissionDialog')
        .close();
    this.completeLastInstall(/*isApproved=*/ false);
  }

  /**
   * Handles accept button of permission review dialog. Continues the
   * installation.
   * @protected
   */
  onAcceptPermissionButtonClicked() {
    this.shadowRoot.querySelector('#shimless3pDiagReviewPermissionDialog')
        .close();
    this.completeLastInstall(/*isApproved=*/ true);
  }

  /**
   * Handles cancel event or back button of error dialog. Close the dialog and
   * ends the launch process.
   * @protected
   */
  onErrorDialogCancelOrBackButtonClicked() {
    this.shadowRoot.querySelector('#shimless3pDiagErrorDialog').close();
    this.completeLaunch();
  }

  /**
   * Launch the 3p diagnostics app. This will ask if users want to install the
   * app from external storage if app files exist, or launch the installed app.
   * @public
   */
  launch3pDiagnostics() {
    if (!loadTimeData.getBoolean('3pDiagnosticsEnabled') ||
        this.hasPendingLaunch) {
      return;
    }

    // If there is no provider or provider is not yet fetched, don't show
    // any UI action and just return.
    if (!this.providerName) {
      return;
    }

    this.hasPendingLaunch = true;
    disableAllButtons(this, /*showBusyStateOverlay=*/ true);

    this.shimlessRmaService.getInstallable3pDiagnosticsAppPath().then(
        (result) => {
          result.appPath ? this.showFindInstallableDialog(result.appPath) :
                           this.show3pDiagnosticsApp();
        });
  }
}

customElements.define(Shimless3pDiagnostics.is, Shimless3pDiagnostics);
