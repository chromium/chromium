// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'startup-settings-card' is the card element containing settings that allow
 * the user to configure the restore apps and pages options on startup.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {PrefsState} from '../common/types.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './startup_settings_card.html.js';

const StartupSettingsCardElementBase =
    RouteObserverMixin(DeepLinkingMixin(PolymerElement));

export class StartupSettingsCardElement extends StartupSettingsCardElementBase {
  static get is() {
    return 'startup-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Used by DeepLinkingMixin to focus this element's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kRestoreAppsAndPages]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      /**
       * List of options for the on startup dropdown menu.
       */
      onStartupDropdownOptions_: {
        type: Array,
        value: () => {
          return [
            {value: 1, name: loadTimeData.getString('onStartupAlways')},
            {value: 2, name: loadTimeData.getString('onStartupAskEveryTime')},
            {value: 3, name: loadTimeData.getString('onStartupDoNotRestore')},
          ];
        },
        readOnly: true,
      },
    };
  }

  prefs: PrefsState;
  private readonly isRevampWayfindingEnabled_: boolean;
  private readonly onStartupDropdownOptions_:
      Array<{value: number, name: string}>;

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute !== routes.SYSTEM_PREFERENCES) {
      return;
    }

    this.attemptDeepLink();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    [StartupSettingsCardElement.is]: StartupSettingsCardElement;
  }
}

customElements.define(
    StartupSettingsCardElement.is, StartupSettingsCardElement);
