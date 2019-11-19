// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  class GoogleAssistantBrowserProxy {
    /** Launches into the Google Assistant app settings. */
    launchGoogleAssistantSettings() {}

    /** Retrain the Assistant voice model. */
    retrainAssistantVoiceModel() {}

    /** Sync the voice model status. */
    syncVoiceModelStatus() {}
  }

  /** @implements {settings.GoogleAssistantBrowserProxy} */
  class GoogleAssistantBrowserProxyImpl {
    /** @override */
    showGoogleAssistantSettings() {
      chrome.send('showGoogleAssistantSettings');
    }

    /** @override */
    retrainAssistantVoiceModel() {
      chrome.send('retrainAssistantVoiceModel');
    }

    /** @override */
    syncVoiceModelStatus() {
      if (loadTimeData.getBoolean('voiceMatchEnabled')) {
        chrome.send('syncVoiceModelStatus');
      }
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(GoogleAssistantBrowserProxyImpl);

  return {
    GoogleAssistantBrowserProxy: GoogleAssistantBrowserProxy,
    GoogleAssistantBrowserProxyImpl: GoogleAssistantBrowserProxyImpl,
  };
});
