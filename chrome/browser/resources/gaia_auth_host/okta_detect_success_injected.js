// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Intercept Ajax responses, detect responses to the password-change endpoint
 * that don't contain any errors.
 */
(function() {
function oktaDetectSuccess() {
  const PARENT_ORIGIN = 'chrome://password-change';

  let messageFromParent;
  function onMessageReceived(event) {
    if (event.origin === PARENT_ORIGIN) {
      messageFromParent = event;
    }
  }
  window.addEventListener('message', onMessageReceived, false);

  function checkResponse(responseUrl, responseData) {
    if (responseUrl.includes('/internal_login/password') &&
        !responseData.match(/"has[A-Za-z]*Errors":true/)) {
      console.info('passwordChangeSuccess');
      messageFromParent.source.postMessage(
          'passwordChangeSuccess', PARENT_ORIGIN);
    }
  }

  const proxied = window.XMLHttpRequest.prototype.send;

  window.XMLHttpRequest.prototype.send = function() {
    this.addEventListener('load', function() {
      checkResponse(this.responseURL, this.response);
    });
    return proxied.apply(this, arguments);
  };
}

/** Run a script in the window context - not isolated as a content-script. */
function runInPageContext(jsFn) {
  const script = document.createElement('script');
  script.type = 'text/javascript';
  script.innerHTML = '(' + jsFn + ')();';
  document.head.prepend(script);
}

/** Wait until DOM is loaded, then run oktaDetectSuccess script. */
function initialize() {
  if (document.body && document.head) {
    console.info('initialize');
    runInPageContext(oktaDetectSuccess);
  } else {
    requestIdleCallback(initialize);
  }
}
requestIdleCallback(initialize);
})();
