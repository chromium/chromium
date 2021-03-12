// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared_css.js';

import {listenOnce} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior} from '../prefs/prefs_behavior.js';

import {DownloadsBrowserProxy, DownloadsBrowserProxyImpl} from './downloads_browser_proxy.js';

Polymer({
  is: 'settings-downloads-page',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

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
  },

  // <if expr="chromeos">
  observers: [
    'handleDownloadLocationChanged_(prefs.download.default_directory.value)'
  ],
  // </if>

  /** @private {?DownloadsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = DownloadsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener('auto-open-downloads-changed', autoOpen => {
      this.autoOpenDownloads_ = autoOpen;
    });

    this.browserProxy_.initializeDownloads();
  },

  /** @private */
  selectDownloadLocation_() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.selectDownloadLocation();
    });
  },

  // <if expr="chromeos">
  /**
   * @private
   */
  handleDownloadLocationChanged_() {
    this.browserProxy_
        .getDownloadLocationText(/** @type {string} */ (
            this.getPref('download.default_directory').value))
        .then(text => {
          this.downloadLocation_ = text;
        });
  },
  // </if>

  /** @private */
  onClearAutoOpenFileTypesTap_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  },
});
