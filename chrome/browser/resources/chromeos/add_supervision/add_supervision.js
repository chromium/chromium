// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * List of URL hosts that can be requested by the webview.
 * @const {!Array<string>}
 */
const ALLOWED_HOSTS = [
  'google.com',
  'gstatic.com',
  'googleapis.com',
  // FIFE avatar images (lh3-lh6). See http://go/fife-domains
  'lh3.googleusercontent.com',
  'lh4.googleusercontent.com',
  'lh5.googleusercontent.com',
  'lh6.googleusercontent.com',
];

/**
 * Returns whether the provided request should be allowed, based on whether
 * its URL matches the list of allowed hosts.
 * @param {!{url: string}} requestDetails Request that is issued by the webview.
 * @return {boolean} Whether the request should be allowed.
 */
function isAllowedRequest(requestDetails) {
  const requestUrl = new URL(requestDetails.url);

  // Only allow HTTPS and hosts that are in the list (or subdomains).
  return requestUrl.protocol == 'https:' &&
      ALLOWED_HOSTS.some(
          (allowedHost) => requestUrl.host == allowedHost ||
              requestUrl.host.endsWith('.' + allowedHost));
}

const addSupervisionHandler =
    addSupervision.mojom.AddSupervisionHandler.getRemote();

Polymer({
  is: 'add-supervision-ui',

  /** Attempts to close the dialog */
  closeDialog_: function() {
    this.server.requestClose();
  },

  /** @override */
  ready: function() {
    // Initialize and listen for online/offline state.
    this.webviewDiv = this.$.webviewDiv;
    this.webviewDiv.hidden = !navigator.onLine;

    this.offlineContentDiv = this.$.offlineContentDiv;
    this.offlineContentDiv.hidden = navigator.onLine;

    window.addEventListener('online', () => {
      this.webviewDiv.hidden = false;
      this.offlineContentDiv.hidden = true;
    });

    window.addEventListener('offline', () => {
      this.webviewDiv.hidden = true;
      this.offlineContentDiv.hidden = false;
    });

    addSupervisionHandler.getOAuthToken().then((result) => {
      const webviewUrl = loadTimeData.getString('webviewUrl');
      const eventOriginFilter = loadTimeData.getString('eventOriginFilter');
      const webview =
          /** @type {!WebView} */ (this.$.webview);

      const accessToken = result.oauthToken;
      const flowType = loadTimeData.getString('flowType');
      const platformVersion = loadTimeData.getString('platformVersion');
      const languageCode = loadTimeData.getString('languageCode');

      const url = new URL(webviewUrl);
      url.searchParams.set('flow_type', flowType);
      url.searchParams.set('platform_version', platformVersion);
      url.searchParams.set('access_token', accessToken);
      url.searchParams.set('hl', languageCode);

      // Allow guest webview content to open links in new windows.
      webview.addEventListener('newwindow', function(e) {
        window.open(e.targetUrl);
      });

      // Block any requests to URLs other than one specified
      // by eventOriginFilter.
      webview.request.onBeforeRequest.addListener(function(details) {
        return {cancel: !isAllowedRequest(details)};
      }, {urls: ['<all_urls>']}, ['blocking']);

      webview.src = url.toString();

      // Set up the server.
      this.server =
          new AddSupervisionAPIServer(webview, url, eventOriginFilter);
    });
  },
});
