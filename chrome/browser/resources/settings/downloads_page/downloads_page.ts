// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {getSearchManager} from '../search_settings.js';
import {PrefServiceObserverMixinLit} from '../settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import type {DownloadsBrowserProxy} from './downloads_browser_proxy.js';
import {DownloadsBrowserProxyImpl} from './downloads_browser_proxy.js';
import {getCss} from './downloads_page.css.js';
import {getHtml} from './downloads_page.html.js';

const SettingsDownloadsPageElementBase =
    PrefServiceObserverMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsDownloadsPageElement extends
    SettingsDownloadsPageElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-downloads-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      autoOpenDownloads_: {type: Boolean},

      downloadDefaultDirectoryPref_: {type: Object},

      // <if expr="is_chromeos">
      /**
       * The download location string that is suitable to display in the UI.
       */
      downloadLocation_: {type: String},
      // </if>

      /**
       * Whether the user can toggle the option to display downloads when
       * they're done.
       */
      downloadBubblePartialViewControlledByPref_: {type: Boolean},
    };
  }

  protected accessor autoOpenDownloads_: boolean = false;
  protected accessor downloadDefaultDirectoryPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined = undefined;

  // <if expr="is_chromeos">
  protected accessor downloadLocation_: string = '';
  // </if>

  protected accessor downloadBubblePartialViewControlledByPref_: boolean =
      loadTimeData.getBoolean('downloadBubblePartialViewControlledByPref');

  private browserProxy_: DownloadsBrowserProxy =
      DownloadsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref(
        'download.default_directory', 'downloadDefaultDirectoryPref_');

    // <if expr="is_chromeos">
    this.addPrefObserver<string>('download.default_directory', pref => {
      this.browserProxy_.getDownloadLocationText(pref.value).then(text => {
        this.downloadLocation_ = text;
      });
    });
    // </if>
  }

  override firstUpdated() {
    this.addWebUiListener(
        'auto-open-downloads-changed', (autoOpen: boolean) => {
          this.autoOpenDownloads_ = autoOpen;
        });

    this.browserProxy_.initializeDownloads();
  }

  protected onChangeDownloadsPathClick_() {
    this.browserProxy_.selectDownloadLocation();
  }

  protected onClearAutoOpenFileTypesClick_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  }

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-downloads-page': SettingsDownloadsPageElement;
  }
}

customElements.define(
    SettingsDownloadsPageElement.is, SettingsDownloadsPageElement);
