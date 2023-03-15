// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './google_drive_subpage.html.js';

const SettingsGoogleDriveSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

class SettingsGoogleDriveSubpageElement extends
    SettingsGoogleDriveSubpageElementBase {
  static get is() {
    return 'settings-google-drive-subpage';
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
    };
  }

  prefs: object;

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.GOOGLE_DRIVE) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-google-drive-subpage': SettingsGoogleDriveSubpageElement;
  }
}

customElements.define(
    SettingsGoogleDriveSubpageElement.is, SettingsGoogleDriveSubpageElement);
