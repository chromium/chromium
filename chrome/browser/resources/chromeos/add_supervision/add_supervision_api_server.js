// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The methods to expose to the client.
 */
const METHOD_LIST = [
  'logOut', 'getInstalledArcApps', 'requestClose', 'notifySupervisionEnabled'
];

/**
 * Class that implements the server side of the AddSupervision postMessage
 * API.  In the case of this API, the Add Supervision WebUI is the server, and
 * the remote website that calls the API  is the client.  This is the opposite
 * of the normal browser/web-server client/server relationship.
 */
class AddSupervisionAPIServer extends PostMessageAPIServer {
  /*
   * @constructor
   * @param {!Element} webviewElement  The <webview> element to listen to as a
   *     client.
   * @param {string} targetURL  The target URL to use for outgoing messages.
   *     This should be the same as the URL loaded in the webview.
   * @param {string} originURLPrefix  The URL prefix to use to filter incoming
   *     messages via the postMessage API.
   */
  constructor(webviewElement, targetURL, originURLPrefix) {
    super(webviewElement, METHOD_LIST, targetURL, originURLPrefix);

    this.addSupervisionHandler_ =
        addSupervision.mojom.AddSupervisionHandler.getRemote();

    this.registerMethod('logOut', this.logOut.bind(this));
    this.registerMethod(
        'getInstalledArcApps', this.getInstalledArcApps.bind(this));
    this.registerMethod('requestClose', this.requestClose.bind(this));
    this.registerMethod(
        'notifySupervisionEnabled', this.notifySupervisionEnabled.bind(this));
  }

  /**
   * Logs out of the device.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  logOut(unused) {
    return this.addSupervisionHandler_.logOut();
  }

  /**
   * @param {!Array} unused Placeholder unused empty parameter.
   * @return {Promise<{
   *         packageNames: !Array<string>,
   *  }>}  a promise whose success result is an array of package names of ARC
   *     apps installed on the device.
   */
  getInstalledArcApps(unused) {
    return this.addSupervisionHandler_.getInstalledArcApps();
  }

  /**
   * Attempts to close the widget hosting the Add Supervision flow.
   * If supervision has already been enabled, this will prompt the
   * user to sign out.
   * @param {!Array} unused Placeholder unused empty parameter.
   * @return {Promise <{closed: boolean}>} If the dialog is not closed
   *     this promise will
   * resolve with boolean result indicating whether the dialog was closed.
   */
  requestClose(unused) {
    return this.addSupervisionHandler_.requestClose();
  }

  /**
   * Signals to the API that supervision has been enabled for the current user.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  notifySupervisionEnabled(unused) {
    return this.addSupervisionHandler_.notifySupervisionEnabled();
  }
}
