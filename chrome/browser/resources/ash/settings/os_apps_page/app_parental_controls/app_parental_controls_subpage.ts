// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App, AppParentalControlsHandlerInterface, AppParentalControlsObserverReceiver} from '../../mojom-webui/app_parental_controls_handler.mojom-webui.js';
import {Router, routes} from '../../router.js';

import {getTemplate} from './app_parental_controls_subpage.html.js';
import {getAppParentalControlsProvider} from './mojo_interface_provider.js';

const SettingsAppParentalControlsSubpageElementBase = I18nMixin(PolymerElement);

export class SettingsAppParentalControlsSubpageElement extends
    SettingsAppParentalControlsSubpageElementBase {
  static get is() {
    return 'settings-app-parental-controls-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appList_: Array,

      // App list that is filtered by searchTerm.
      filteredAppList_: {
        type: Array,
        value: () => [],
        computed: 'computeAppList_(appList_, searchTerm)',
      },

      isVerified: {
        type: Boolean,
        value: false,
      },

      searchTerm: String,
    };
  }

  searchTerm: string;
  private appList_: App[] = [];
  private isVerified: boolean;
  private filteredAppList_: App[];
  private mojoInterfaceProvider: AppParentalControlsHandlerInterface;
  private observerReceiver: AppParentalControlsObserverReceiver|null;

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
    this.observerReceiver = null;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.mojoInterfaceProvider.getApps().then((result) => {
      this.appList_ = result.apps;
    });

    this.observerReceiver = new AppParentalControlsObserverReceiver(this);
    this.mojoInterfaceProvider.addObserver(
        this.observerReceiver.$.bindNewPipeAndPassRemote());

    if (!this.isVerified) {
      // Redirect to the apps page if the PIN is not verified.
      setTimeout(() => {
        Router.getInstance().navigateTo(routes.APPS);
      });
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.observerReceiver!.$.close();
  }

  private computeAppList_(apps: App[], searchTerm: string): App[] {
    let filteredApps: App[];
    if (searchTerm) {
      filteredApps = apps.filter((app: App) => {
        assert(app.title);
        return app.title.toLowerCase().includes(searchTerm.toLowerCase());
      });
      return filteredApps;
    }
    return apps;
  }

  private alphabeticalSort_(first: App, second: App): number {
    return first.title!.localeCompare(second.title!);
  }

  private isAppListEmpty_(apps: App[]): boolean {
    return apps.length === 0;
  }

  private getBlockedAppsCountString_(apps: App[]): string {
    const blockedAppsCount = apps.filter(app => app.isBlocked).length;
    const appListCount = apps.length;

    return this.i18n(
        'appParentalControlsBlockedAppsCountText', blockedAppsCount,
        appListCount);
  }

  private shouldShowBlockedAppsCountString_(apps: App[], searchString: string):
      boolean {
    return apps.length > 0 && !searchString;
  }

  onAppInstalledOrUpdated(updatedApp: App): void {
    // Using Polymer mutation methods do not properly handle splice updates with
    // object that have deep properties. Create and assign a copy list instead.
    const appList = Array.from(this.appList_);
    const foundIdx = this.appList_.findIndex(app => {
      return app.id === updatedApp.id;
    });

    // If app is not found, then it is a newly installed app.
    if (foundIdx === -1) {
      appList.push(updatedApp);
      this.appList_ = appList;
      return;
    }

    const blockStateChanged =
        this.appList_[foundIdx].isBlocked !== updatedApp.isBlocked;
    if (blockStateChanged) {
      appList[foundIdx] = updatedApp;
      this.appList_ = appList;
    }
  }

  onAppUninstalled(updatedApp: App): void {
    // Using Polymer mutation methods do not properly handle splice updates with
    // object that have deep properties. Create and assign a copy list instead.
    const appList = Array.from(this.appList_);
    const foundIdx = this.appList_.findIndex(app => {
      return app.id === updatedApp.id;
    });

    if (foundIdx === -1) {
      console.error(
          'app-controls: Attempting to remove app: ', updatedApp.id,
          ' which is not installed.');
      return;
    }

    appList.splice(foundIdx, 1);
    this.appList_ = appList;
  }
}

customElements.define(
    SettingsAppParentalControlsSubpageElement.is,
    SettingsAppParentalControlsSubpageElement);
