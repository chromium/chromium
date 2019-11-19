// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Plugin VM section
 * to manage the Plugin VM.
 */
cr.define('settings', function() {
  /** @interface */
  class PluginVmBrowserProxy {
    /**
     * @param {!Array<string>} paths Paths to sanitze.
     * @return {!Promise<!Array<string>>} Text to display in UI.
     */
    getPluginVmSharedPathsDisplayText(paths) {}

    /**
     * @param {string} vmName VM to stop sharing path with.
     * @param {string} path Path to stop sharing.
     */
    removePluginVmSharedPath(vmName, path) {}
  }

  /** @implements {settings.PluginVmBrowserProxy} */
  class PluginVmBrowserProxyImpl {
    /** @override */
    getPluginVmSharedPathsDisplayText(paths) {
      return cr.sendWithPromise('getPluginVmSharedPathsDisplayText', paths);
    }

    /** @override */
    removePluginVmSharedPath(vmName, path) {
      chrome.send('removePluginVmSharedPath', [vmName, path]);
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(PluginVmBrowserProxyImpl);

  return {
    PluginVmBrowserProxy: PluginVmBrowserProxy,
    PluginVmBrowserProxyImpl: PluginVmBrowserProxyImpl,
  };
});
