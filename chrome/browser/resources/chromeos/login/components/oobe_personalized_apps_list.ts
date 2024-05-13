// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_personalized_apps_list.html.js';

/**
 * Data that is passed to the component during initialization.
 */
export interface AppsListApp {
  appId: string;
  name: string;
  subname: string;
  package_name: string;
  icon: string;
  selected: boolean;
}

export interface CategoriesAppsMap {
  [category: string]: OobeAppsListData;
}

export interface OobeAppsListData extends Array<AppsListApp> {}

const OobePersonalizedAppsListBase = PolymerElement;

export class OobePersonalizedAppsList extends OobePersonalizedAppsListBase {
  static get is() {
    return 'oobe-personalized-apps-list' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of apps displayed converted from the map.
       */
      appsList: {
        type: Array,
        value: [],
        notify: true,
      },
      /**
       * List of apps displayed converted from the map.
       */
      catgoriesMapApps: {
        type: Object,
      },
      /**
       * Number of selected apps.
       */
      selectedAppsCount: {
        type: Number,
        value: 0,
        notify: true,
      },
      /**
       * Number of loaded icons apps.
       */
      loadedIconsCount: {
        type: Number,
        value: 0,
      },
      /**
       * Number of dom repeat rendered items.
       */
      itemRendered: {
        type: Number,
        value: 0,
        observer: 'itemRenderedChanged',
      },
    };
  }

  private catgoriesMapApps: CategoriesAppsMap;
  private appsList: OobeAppsListData;
  private selectedAppsCount: number;
  private loadedIconsCount: number;
  private itemRendered: number;

  /**
   * Initialize the list of categories.
   */
  init(data: CategoriesAppsMap): void {
    this.catgoriesMapApps = data;
    this.selectedAppsCount = 0;
    this.loadedIconsCount = 0;
    this.itemRendered = 0;
    for (const key in this.catgoriesMapApps) {
      this.catgoriesMapApps[key].forEach(element => {
        this.appsList.push(element);
      });
    }
  }

  itemRenderedChanged(): void {
    if (this.itemRendered === this.appsList.length &&
        this.loadedIconsCount === this.appsList.length) {
      this.dispatchEvent(new CustomEvent(
          'apps-icons-loaded', {bubbles: true, composed: true}));
    }
  }

  /**
   * Return if categories title should be shown.
   */
  private shouldShowCategoriesTitle(): boolean {
    return !(Object.keys(this.catgoriesMapApps).length > 1);
  }

  /**
   * Return the list of selected apps.
   */
  getAppsSelected(): string[] {
    const packageNames: string[] = [];
    this.appsList.forEach((app) => {
      if (app.selected) {
        packageNames.push(app.package_name);
      }
    });
    return packageNames;
  }

  private getIconUrl(iconUrl: string): string {
    return iconUrl;
  }

  getCategories(data: CategoriesAppsMap): string[] {
    return Object.keys(data);
  }

  getApps(key: string): OobeAppsListData {
    return this.catgoriesMapApps[key];
  }

  private updateCount(): void {
    let appsSelected = 0;
    this.appsList.forEach((app) => {
      if (app.selected) {
        appsSelected++;
      }
    });
    this.selectedAppsCount = appsSelected;
  }

  private getAppId(appId: string): string {
    return 'cr-button-' + appId;
  }

  private getWebViewId(appId: string): string {
    return 'webview-' + appId;
  }

  private onIconLoaded(): void {
    this.loadedIconsCount += 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobePersonalizedAppsList.is]: OobePersonalizedAppsList;
  }
}

customElements.define(OobePersonalizedAppsList.is, OobePersonalizedAppsList);
