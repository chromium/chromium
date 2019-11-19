// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-downloads-page prefs="{{prefs}}">
 *      </settings-downloads-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-downloads-page',

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!DownloadsPageVisibility}
     */
    pageVisibility: Object,

    /** @private */
    autoOpenDownloads_: {
      type: Boolean,
      value: false,
    },

    // <if expr="chromeos">
    /**
     * The download location string that is suitable to display in the UI.
     */
    downloadLocation_: String,
    // </if>

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        // <if expr="chromeos">
        if (settings.routes.SMB_SHARES) {
          map.set(settings.routes.SMB_SHARES.path, '#smbShares');
        }
        // </if>
        return map;
      },
    },

  },

  // <if expr="chromeos">
  observers: [
    'handleDownloadLocationChanged_(prefs.download.default_directory.value)'
  ],
  // </if>

  /** @private {?settings.DownloadsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.DownloadsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('auto-open-downloads-changed', autoOpen => {
      this.autoOpenDownloads_ = autoOpen;
    });

    this.browserProxy_.initializeDownloads();
  },

  /** @private */
  selectDownloadLocation_: function() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.selectDownloadLocation();
    });
  },

  // <if expr="chromeos">
  /** @private */
  onTapSmbShares_: function() {
    settings.navigateTo(settings.routes.SMB_SHARES);
  },

  /**
   * @private
   */
  handleDownloadLocationChanged_: function() {
    this.browserProxy_
        .getDownloadLocationText(/** @type {string} */ (
            this.getPref('download.default_directory').value))
        .then(text => {
          this.downloadLocation_ = text;
        });
  },
  // </if>

  /** @private */
  onClearAutoOpenFileTypesTap_: function() {
    this.browserProxy_.resetAutoOpenFileTypes();
  },
});
