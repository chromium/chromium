// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview
 * Browser Proxy for Parental Controls functions.
 */

cr.define('parental_controls', function() {
  /** @interface */
  /* #export */ class ParentalControlsBrowserProxy {
    /**
     * Shows the Add Supervsion dialog.
     */
    showAddSupervisionDialog() {}

    /**
     * Launches an app that shows the Family Link Settings.  Depending
     * on whether the Family Link Helper app is available, this might
     * launch the app, or take some kind of backup/default action.
     */
    launchFamilyLinkSettings() {}
  }

  /** @implements {parental_controls.ParentalControlsBrowserProxy} */
  /* #export */ class ParentalControlsBrowserProxyImpl {
    /** @override */
    showAddSupervisionDialog() {
      chrome.send('showAddSupervisionDialog');
    }

    /** @override */
    launchFamilyLinkSettings() {
      chrome.send('launchFamilyLinkSettings');
    }
  }

  cr.addSingletonGetter(ParentalControlsBrowserProxyImpl);

  // #cr_define_end
  return {
    ParentalControlsBrowserProxy: ParentalControlsBrowserProxy,
    ParentalControlsBrowserProxyImpl: ParentalControlsBrowserProxyImpl,
  };
});
