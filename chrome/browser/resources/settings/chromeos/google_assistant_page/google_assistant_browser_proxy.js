// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  /* #export */ class GoogleAssistantBrowserProxy {
    /** Launches into the Google Assistant app settings. */
    showGoogleAssistantSettings() {}

    /** Retrain the Assistant voice model. */
    retrainAssistantVoiceModel() {}

    /** Sync the voice model status. */
    syncVoiceModelStatus() {}
  }

  /** @implements {settings.GoogleAssistantBrowserProxy} */
  /* #export */ class GoogleAssistantBrowserProxyImpl {
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
      chrome.send('syncVoiceModelStatus');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(GoogleAssistantBrowserProxyImpl);

  // #cr_define_end
  return {
    GoogleAssistantBrowserProxy: GoogleAssistantBrowserProxy,
    GoogleAssistantBrowserProxyImpl: GoogleAssistantBrowserProxyImpl,
  };
});
