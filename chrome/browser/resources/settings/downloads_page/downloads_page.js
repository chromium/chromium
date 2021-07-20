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
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs/prefs_behavior.js';

import {DownloadsBrowserProxy, DownloadsBrowserProxyImpl} from './downloads_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const SettingsDownloadsPageElementBase =
    mixinBehaviors([WebUIListenerBehavior, PrefsBehavior], PolymerElement);

/** @polymer */
class SettingsDownloadsPageElement extends SettingsDownloadsPageElementBase {
  static get is() {
    return 'settings-downloads-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private */
      showConnection_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      connectionLearnMoreLink_: {
        type: String,
        value:
            'https://chromeenterprise.google/policies/?policy=SendDownloadToCloudEnterpriseConnector',
      },

      /**
       * @private
       * The connection account info object. The definition is based on
       * chrome/browser/enterprise/connectors/file_system/signin_experience.cc:
       * GetFileSystemConnectorLinkedAccountInfoForSettingsPage()
       * @type {{
       *  linked: boolean,
       *  account: {name: string, login: string},
       *  folder: {name: string, link: string},
       * }}
       */
      connectionAccountInfo_: {
        type: Object,
        notify: true,
      },

      /** @private */
      connectionSetupInProgress_: {
        type: Boolean,
        value: false,
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
    };
  }

  // <if expr="chromeos">
  static get observers() {
    return [
      'handleDownloadLocationChanged_(prefs.download.default_directory.value)'
    ];
  }
  // </if>

  /** @override */
  constructor() {
    super();

    /** @private {!DownloadsBrowserProxy} */
    this.browserProxy_ = DownloadsBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener('auto-open-downloads-changed', autoOpen => {
      this.autoOpenDownloads_ = autoOpen;
    });

    this.addWebUIListener(
        'downloads-connection-policy-changed', downloadsConnectionEnabled => {
          this.showConnection_ = downloadsConnectionEnabled;
        });

    this.addWebUIListener('downloads-connection-link-changed', accountInfo => {
      this.connectionAccountInfo_ = accountInfo;
      this.connectionSetupInProgress_ = false;
    });

    this.browserProxy_.initializeDownloads();
  }

  /** @private */
  onLinkDownloadsConnectionClick_() {
    this.connectionSetupInProgress_ = true;
    this.browserProxy_.setDownloadsConnectionAccountLink(true);
  }

  /** @private */
  onUnlinkDownloadsConnectionClick_() {
    this.connectionSetupInProgress_ = true;
    this.browserProxy_.setDownloadsConnectionAccountLink(false);
  }

  /** @private */
  selectDownloadLocation_() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.selectDownloadLocation();
    });
  }

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
  }
  // </if>

  /** @private */
  onClearAutoOpenFileTypesTap_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  }
}

customElements.define(
    SettingsDownloadsPageElement.is, SettingsDownloadsPageElement);
