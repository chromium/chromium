// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{path: string,
 *            pathDisplayText: string}}
 */
let CrostiniSharedPath;

/**
 * @fileoverview A helper object used by the "Linux Apps" (Crostini) section
 * to install and uninstall Crostini.
 */
cr.define('settings', function() {
  /** @interface */
  class CrostiniBrowserProxy {
    /* Show crostini installer. */
    requestCrostiniInstallerView() {}

    /* Show remove crostini dialog. */
    requestRemoveCrostini() {}

    /**
     * @param {!Array<string>} paths Paths to sanitze.
     * @return {!Promise<!Array<string>>} Text to display in UI.
     */
    getCrostiniSharedPathsDisplayText(paths) {}

    /** @param {string} path Path to stop sharing. */
    removeCrostiniSharedPath(path) {}
  }

  /** @implements {settings.CrostiniBrowserProxy} */
  class CrostiniBrowserProxyImpl {
    /** @override */
    requestCrostiniInstallerView() {
      chrome.send('requestCrostiniInstallerView');
    }

    /** @override */
    requestRemoveCrostini() {
      chrome.send('requestRemoveCrostini');
    }

    /** @override */
    getCrostiniSharedPathsDisplayText(paths) {
      return cr.sendWithPromise('getCrostiniSharedPathsDisplayText', paths);
    }

    /** @override */
    removeCrostiniSharedPath(path) {
      chrome.send('removeCrostiniSharedPath', [path]);
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(CrostiniBrowserProxyImpl);

  return {
    CrostiniBrowserProxy: CrostiniBrowserProxy,
    CrostiniBrowserProxyImpl: CrostiniBrowserProxyImpl,
  };
});
