// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-smart-inputs-page' is the settings sub-page
 * to provide users with assistive or expressive input options.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './smart_inputs_page.html.js';

const OsSettingsSmartInputsPageElementBase =
    RouteObserverMixin(PrefsMixin(I18nMixin(DeepLinkingMixin(PolymerElement))));

export class OsSettingsSmartInputsPageElement extends
    OsSettingsSmartInputsPageElementBase {
  static get is() {
    return 'os-settings-smart-inputs-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(b/265554350): Remove this property from properties() as it is
      // already specified in PrefsMixin.
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      allowEmojiSuggestion_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowEmojiSuggestion');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kShowEmojiSuggestions,
        ]),
      },
    };
  }

  // Public API: Bidirectional data flow.
  // override prefs: any;  // From PrefsMixin.

  // Internal properties for mixins.
  // From DeepLinkingMixin.
  // override supportedSettingIds = new Set<Setting>([
  //   Setting.kShowEmojiSuggestions,
  // ]);

  // loadTimeData flags.
  private allowEmojiSuggestion_: boolean;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_SMART_INPUTS) {
      return;
    }

    this.attemptDeepLink();
  }
}

customElements.define(
    OsSettingsSmartInputsPageElement.is, OsSettingsSmartInputsPageElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsSmartInputsPageElement.is]: OsSettingsSmartInputsPageElement;
  }
}
