// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import './office_page.js';
import './smb_shares_page.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_files_page.html.js';

const OsSettingsFilesPageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

class OsSettingsFilesPageElement extends OsSettingsFilesPageElementBase {
  static get is() {
    return 'os-settings-files-page';
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

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kGoogleDriveConnection]),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SMB_SHARES) {
            map.set(routes.SMB_SHARES.path, '#smbShares');
          }
          return map;
        },
      },

      /** @private */
      showOfficeSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOfficeSettings');
        },
      },
    };
  }

  prefs: Object;
  private focusConfig_: Map<string, string>;

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.FILES) {
      return;
    }

    this.attemptDeepLink();
  }

  private onTapSmbShares_() {
    Router.getInstance().navigateTo(routes.SMB_SHARES);
  }

  private onTapOffice_() {
    Router.getInstance().navigateTo(routes.OFFICE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-files-page': OsSettingsFilesPageElement;
  }
}

customElements.define(
    OsSettingsFilesPageElement.is, OsSettingsFilesPageElement);
