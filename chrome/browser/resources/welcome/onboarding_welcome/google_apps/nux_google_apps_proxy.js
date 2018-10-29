// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /** @interface */
  class NuxGoogleAppsProxy {
    /**
     * Google app IDs are local to the list of Google apps, so their icon must
     * be cached by the handler that provided the IDs.
     * @param {number} appId
     */
    cacheBookmarkIcon(appId) {}

    /**
     * Returns a promise for an array of Google apps.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getGoogleAppsList() {}
  }

  /** @implements {nux.NuxGoogleAppsProxy} */
  class NuxGoogleAppsProxyImpl {
    /** @override */
    cacheBookmarkIcon(appId) {
      chrome.send('cacheGoogleAppIcon', [appId]);
    }

    /** @override */
    getGoogleAppsList() {
      return cr.sendWithPromise('getGoogleAppsList');
    }
  }

  cr.addSingletonGetter(NuxGoogleAppsProxyImpl);

  return {
    NuxGoogleAppsProxy: NuxGoogleAppsProxy,
    NuxGoogleAppsProxyImpl: NuxGoogleAppsProxyImpl,
  };
});
