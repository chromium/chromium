// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-stylus' is the settings subpage with stylus-specific settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import type {PrefsState} from '../common/types.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {type Route, routes} from '../router.js';

import type {NoteAppInfo} from './device_page_browser_proxy.js';
import {type DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {getTemplate} from './stylus.html.js';

export interface SettingsStylusElement {
  $: {
    selectApp: HTMLSelectElement,
  };
}

const FIND_MORE_APPS_URL = 'https://play.google.com/store/apps/' +
    'collection/promotion_30023cb_stylus_apps';

const SettingsStylusElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

export class SettingsStylusElement extends SettingsStylusElementBase {
  static get is() {
    return 'settings-stylus';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Note taking apps the user can pick between.
       */
      appChoices_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * True if the device has an internal stylus.
       */
      hasInternalStylus_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('hasInternalStylus');
        },
        readOnly: true,
      },

      /**
       * Currently selected note taking app.
       */
      selectedApp_: {
        type: Object,
        value: null,
      },

      /**
       * True if the ARC container has not finished starting yet.
       */
      waitingForAndroid_: {
        type: Boolean,
        value: false,
      },
    };
  }

  prefs: PrefsState;

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kStylusToolsInShelf,
    Setting.kStylusNoteTakingApp,
  ]);

  private appChoices_: NoteAppInfo[];
  private browserProxy_: DevicePageBrowserProxy;
  private readonly hasInternalStylus_: boolean;
  private selectedApp_: NoteAppInfo|null;
  private waitingForAndroid_: boolean;

  constructor() {
    super();

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.browserProxy_.setNoteTakingAppsUpdatedCallback(
        this.onNoteAppsUpdated_.bind(this));
    this.browserProxy_.requestNoteTakingApps();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.STYLUS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Finds note app info with the provided app id.
   */
  private findApp_(id: string): NoteAppInfo|null {
    return this.appChoices_.find((app) => app.value === id) || null;
  }

  private onSelectedAppChanged_(): void {
    const app = this.findApp_(this.$.selectApp.value);
    this.selectedApp_ = app;

    if (app && !app.preferred) {
      this.browserProxy_.setPreferredNoteTakingApp(app.value);
      recordSettingChange(Setting.kStylusNoteTakingApp);
    }
  }

  private onNoteAppsUpdated_(apps: NoteAppInfo[], waitingForAndroid: boolean):
      void {
    this.waitingForAndroid_ = waitingForAndroid;
    this.appChoices_ = apps;

    // Wait until app selection UI is updated before setting the selected app.
    microTask.run(this.onSelectedAppChanged_.bind(this));
  }

  private showNoApps_(apps: NoteAppInfo[], waitingForAndroid: boolean):
      boolean {
    return apps.length === 0 && !waitingForAndroid;
  }

  private showApps_(apps: NoteAppInfo[], waitingForAndroid: boolean): boolean {
    return apps.length > 0 && !waitingForAndroid;
  }

  private onFindAppsClick_(): void {
    this.browserProxy_.showPlayStore(FIND_MORE_APPS_URL);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-stylus': SettingsStylusElement;
  }
}

customElements.define(SettingsStylusElement.is, SettingsStylusElement);
