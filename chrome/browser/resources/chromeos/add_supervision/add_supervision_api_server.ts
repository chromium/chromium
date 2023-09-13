// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';

import {AddSupervisionHandler, AddSupervisionHandlerRemote} from './add_supervision.mojom-webui.js';
import {AddSupervisionUi, isLocalHostForTesting} from './add_supervision_ui.js';

/**
 * Class that implements the server side of the AddSupervision postMessage
 * API.  In the case of this API, the Add Supervision WebUI is the server, and
 * the remote website that calls the API  is the client.  This is the opposite
 * of the normal browser/web-server client/server relationship.
 */
export class AddSupervisionApiServer extends PostMessageApiServer {
  private ui: AddSupervisionUi;
  private addSupervisionHandler: AddSupervisionHandlerRemote;

  constructor(
      ui: AddSupervisionUi, webviewElement: Element, targetURL: string,
      originURLPrefix: string) {
    super(webviewElement, targetURL, originURLPrefix);

    this.ui = ui;

    this.addSupervisionHandler = AddSupervisionHandler.getRemote();

    this.registerMethod('logOut', this.logOut.bind(this));
    this.registerMethod(
        'getInstalledArcApps', this.getInstalledArcApps.bind(this));
    this.registerMethod('requestClose', this.requestClose.bind(this));
    this.registerMethod(
        'notifySupervisionEnabled', this.notifySupervisionEnabled.bind(this));
    this.registerMethod('setCloseOnEscape', this.setCloseOnEscape.bind(this));
  }

  override initialize() {
    // The server cannot communicate with the mock webview used
    // in the browser test, so skip initialization during tests.
    if (isLocalHostForTesting(this.targetUrl())) {
      return;
    }
    super.initialize();
  }

  override onInitializationError() {
    this.ui.showErrorPage();
  }

  logOut(): void {
    return this.addSupervisionHandler.logOut();
  }

  /**
   * Returns a promise whose success result is an array of package names of ARC
   * apps installed on the device.
   */
  getInstalledArcApps(): Promise<{packageNames: string[]}> {
    return this.addSupervisionHandler.getInstalledArcApps();
  }

  /**
   * Attempts to close the widget hosting the Add Supervision flow.
   * If supervision has already been enabled, this will prompt the
   * user to sign out. If the dialog is not closed this promise will
   * resolve with boolean result indicating whether the dialog was closed.
   */
  requestClose(): Promise<{closed: boolean}> {
    return this.addSupervisionHandler.requestClose();
  }

  /**
   * Signals to the API that supervision has been enabled for the current user.
   */
  notifySupervisionEnabled(): void {
    return this.addSupervisionHandler.notifySupervisionEnabled();
  }

  /**
   * Configures whether the Add Supervision dialog should close when
   * the user presses the Escape key.
   */
  setCloseOnEscape(params: any[]): void {
    // Param 0 is a <boolean> that denotes whether the dialog should close.
    const enabled = params[0];
    return this.addSupervisionHandler.setCloseOnEscape(enabled);
  }
}
