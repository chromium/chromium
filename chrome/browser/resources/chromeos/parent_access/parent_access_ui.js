// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './parent_access_ui.mojom-lite.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebviewManager} from 'chrome://resources/js/webview_manager.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ParentAccessController} from './parent_access_controller.js';

const parentAccessUIHandler =
    parentAccessUi.mojom.ParentAccessUIHandler.getRemote();

/**
 * List of URL hosts that can be requested by the webview. The
 * webview URL's host is implicitly included in this list.
 * @const {!Array<string>}
 */
const ALLOWED_HOSTS = [
  'googleapis.com',
  'gstatic.com',
  'googleusercontent.com',
  'google.com',
];

Polymer({
  is: 'parent-access-ui',
  _template: html`{__html_template__}`,
  /**
   * Returns whether the provided request should be allowed.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  isAllowedRequest(url) {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl_);

    // Allow non https only for requests to a local development server webview
    // URL, which would have been specified at the command line.
    if (requestUrl.host === webviewUrl.host) {
      return true;
    }

    // Otherwise, all requests should be https and in the ALLOWED_HOSTS list.
    const requestIsHttps = requestUrl.protocol === 'https:';
    const requestIsInAllowedHosts = ALLOWED_HOSTS.some(
        (allowedHost) => requestUrl.host == allowedHost ||
            requestUrl.host.endsWith(allowedHost));

    return requestIsHttps && requestIsInAllowedHosts;
  },

  /**
   * Returns whether the provided request should receive auth headers.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  shouldReceiveAuthHeader(url) {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl_);

    // Only the webviewUrl URL should receive the auth header, because for
    // security reasons, we shouldn't distribute the OAuth token any more
    // broadly that strictly necessary for the widget to function, thereby
    // minimizing the attack surface for the token.
    return requestUrl.host === webviewUrl.host;
  },

  /** @override */
  ready() {
    this.configureUi().then(
        () => {/* success */},
        origin => {/* TODO(b/200187536): show error page. */});
  },

  async configureUi() {
    /**
     * @private {string} The initial URL for the webview.
     */
    this.webviewUrl_ = loadTimeData.getString('webviewUrl');

    const eventOriginFilter = loadTimeData.getString('eventOriginFilter');

    const oauthFetchResult = await parentAccessUIHandler.getOAuthToken();
    if (oauthFetchResult.status !=
        parentAccessUi.mojom.GetOAuthTokenStatus.kSuccess) {
      // TODO(b/200187536): show error page.
      return;
    }

    const webview =
        /** @type {!WebView} */ (this.$.webview);
    const accessToken = oauthFetchResult.oauthToken;

    // Set up the WebviewManager to handle the configuration and
    // access control for the webview.
    this.webview_manager_ = new WebviewManager(webview);
    this.webview_manager_.setAccessToken(accessToken, (url) => {
      return this.shouldReceiveAuthHeader(url);
    });
    this.webview_manager_.setAllowRequestFn((url) => {
      return this.isAllowedRequest(url);
    });

    // Setting the src of the webview triggers the loading process.
    const url = new URL(this.webviewUrl_);
    webview.src = url.toString();

    // Set up the controller. It will automatically start the initialization
    // handshake with the hosted content.
    this.server =
        new ParentAccessController(webview, url.toString(), eventOriginFilter);

    const parentAccessResult = await Promise.race([
      this.server.whenParentAccessResult(),
      this.server.whenInitializationError(),
    ]);

    // Notify ParentAccessUIHandler that we received a result.
    const decodedParentAccessResult =
        await parentAccessUIHandler.onParentAccessResult(parentAccessResult);

    switch (decodedParentAccessResult.status) {
      case parentAccessUi.mojom.ParentAccessResultStatus.kParentVerified:
        this.dispatchEvent(new CustomEvent('show-after', {
          bubbles: true,
          composed: true,
        }));
        break;

      // ConsentDeclined result status is not currently supported, so show an
      // error.
      case parentAccessUi.mojom.ParentAccessResultStatus.kConsentDeclined:
      case parentAccessUi.mojom.ParentAccessResultStatus.kError:
      default:
        // TODO(b/200187536): show error page
        break;
    }

  },
});
