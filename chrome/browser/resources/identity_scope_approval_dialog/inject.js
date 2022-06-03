// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const injectCode =
    `const identityAppExtensionId = 'ahjaciijnoiaklcomgnblndopackapon';
if (!window.OAuthConsent) {
  window.OAuthConsent = {};
}
if (!window.OAuthConsent.setConsentResult) {
  window.OAuthConsent.setConsentResult = function(result) {
    chrome.runtime.sendMessage(identityAppExtensionId, {consentResult: result});
  };
}`;

const script = document.createElement('script');
script.innerText = injectCode;
document.documentElement.prepend(script);
