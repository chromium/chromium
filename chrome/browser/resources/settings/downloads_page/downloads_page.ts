// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import type {DownloadsBrowserProxy} from './downloads_browser_proxy.js';
import {DownloadsBrowserProxyImpl} from './downloads_browser_proxy.js';
import {getTemplate} from './downloads_page.html.js';

const SettingsDownloadsPageElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));

export class SettingsDownloadsPageElement extends
    SettingsDownloadsPageElementBase {
  static get is() {
    return 'settings-downloads-page';
  }

  static get template() {
    return getTemplate();
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

      autoOpenDownloads_: {
        type: Boolean,
        value: false,
      },

      // <if expr="chromeos_ash">
      /**
       * The download location string that is suitable to display in the UI.
       */
      downloadLocation_: String,
      // </if>

      /**
       * Whether the user can toggle the option to display downloads when
       * they're done.
       */
      downloadBubblePartialViewControlledByPref_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'downloadBubblePartialViewControlledByPref');
        },
      },
    };
  }

  // <if expr="chromeos_ash">
  static get observers() {
    return [
      'handleDownloadLocationChanged_(prefs.download.default_directory.value)',
    ];
  }
  // </if>


  private autoOpenDownloads_: boolean;

  // <if expr="chromeos_ash">
  private downloadLocation_: string;
  // </if>

  private downloadBubblePartialViewControlledByPref_: boolean;

  private browserProxy_: DownloadsBrowserProxy =
      DownloadsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUiListener(
        'auto-open-downloads-changed', (autoOpen: boolean) => {
          this.autoOpenDownloads_ = autoOpen;
        });

    this.browserProxy_.initializeDownloads();
  }

  private selectDownloadLocation_() {
    this.browserProxy_.selectDownloadLocation();
  }

  // <if expr="chromeos_ash">
  private handleDownloadLocationChanged_() {
    this.browserProxy_
        .getDownloadLocationText(
            this.getPref<string>('download.default_directory').value)
        .then(text => {
          this.downloadLocation_ = text;
        });
  }
  // </if>

  private onClearAutoOpenFileTypesClick_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-downloads-page': SettingsDownloadsPageElement;
  }
}

customElements.define(
    SettingsDownloadsPageElement.is, SettingsDownloadsPageElement);
