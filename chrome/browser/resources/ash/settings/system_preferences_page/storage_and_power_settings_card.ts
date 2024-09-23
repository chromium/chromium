// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-and-power-settings-card' is the card element containing storage and
 * power settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './storage_and_power_settings_card.html.js';

const StorageAndPowerSettingsCardElementBase =
    RouteOriginMixin(I18nMixin(PolymerElement));

export class StorageAndPowerSettingsCardElement extends
    StorageAndPowerSettingsCardElementBase {
  static get is() {
    return 'storage-and-power-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldShowStorageRow_: {
        type: Boolean,
        value: () => {
          // TODO(crbug.com/40587075): Show an explanatory message instead.
          return !loadTimeData.getBoolean('isDemoSession');
        },
        readOnly: true,
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              storage: 'os-settings:storage',
              power: 'os-settings:power',
            };
          }
          return {
            storage: '',
            power: '',
          };
        },
      },
    };
  }

  private rowIcons_: Record<string, string>;
  private shouldShowStorageRow_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.SYSTEM_PREFERENCES;
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.STORAGE, '#storageRow');
    this.addFocusConfig(routes.POWER, '#powerRow');
  }

  private getHeaderText_(): string {
    // The `storageAndPowerTitle` string is only defined when the
    // OsSettingsRevampWayfinding feature flag is enabled. Avoid using $i18n{}
    // templating in HTML to avoid crashes when the feature is disabled.
    return this.i18n('storageAndPowerTitle');
  }

  private showStorageSubpage_(): void {
    Router.getInstance().navigateTo(routes.STORAGE);
  }

  private showPowerSubpage_(): void {
    Router.getInstance().navigateTo(routes.POWER);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [StorageAndPowerSettingsCardElement.is]: StorageAndPowerSettingsCardElement;
  }
}

customElements.define(
    StorageAndPowerSettingsCardElement.is, StorageAndPowerSettingsCardElement);
