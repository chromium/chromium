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

export interface CategoryItem {
  id: string;
  count: number;
}

export interface CategoriesAppsMap {
  [category: string]: OobeAppsListData;
}

export interface CategoriesItemList extends Array<CategoryItem> {}

export interface OobeAppsListData extends Array<AppsListApp> {}

export interface CategoryAppsItem {
  name: string;
  apps: AppsListApp[];
}

export interface CategoryAppsItems extends Array<CategoryAppsItem> {}

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
      categoriesMapApps: {
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
      categoriesItemRendered: {
        type: Object,
        value: [],
      },
    };
  }

  private categoriesMapApps: CategoriesAppsMap;
  private appsList: OobeAppsListData;
  private selectedAppsCount: number;
  private loadedIconsCount: number;
  private categoriesItemRendered: CategoriesItemList;

  // Observe the name sub-property on the user object
  static get observers() {
    return ['itemRenderedChanged(categoriesItemRendered.*)'];
  }

  /**
   * Initialize the list of categories.
   */
  init(data: CategoryAppsItems): void {
    this.categoriesMapApps = {};
    for (const categoryAppsItem of data) {
      this.categoriesMapApps[categoryAppsItem.name] = categoryAppsItem.apps;
    }
    this.selectedAppsCount = 0;
    this.loadedIconsCount = 0;
    this.categoriesItemRendered = [];
    this.appsList = [];
    for (const key in this.categoriesMapApps) {
      this.categoriesItemRendered.push({'id': key, 'count': 0});
      this.categoriesMapApps[key].forEach(element => {
        this.appsList.push(element);
      });
    }
  }

  reset(): void {
    this.categoriesMapApps = {};
    this.selectedAppsCount = 0;
    this.loadedIconsCount = 0;
    this.categoriesItemRendered = [];
    this.appsList = [];
    this.resetScroll();
  }

  /**
   * Reset scroll position to the top between screen's data changes.
   */
  private resetScroll(): void {
    const appsList = this.shadowRoot?.querySelector('#personalizedApps');
    if (appsList) {
      appsList.scrollTop = 0;
    }
  }

  itemRenderedChanged(_itemRendered: CategoriesItemList): void {
    let count = 0;
    this.categoriesItemRendered.forEach((element) => {
      count += element.count;
    });
    if (this.appsList.length !== 0 && count === this.appsList.length) {
      this.setWebviewStyle();
      this.dispatchEvent(
          new CustomEvent('icons-loaded', {bubbles: true, composed: true}));
    }
  }

  setWebviewStyle(): void {
    const iconWebviews =
        this.shadowRoot?.querySelectorAll<chrome.webviewTag.WebView>(
            '.app-icon');
    if (iconWebviews) {
      for (const iconWebview of iconWebviews) {
        this.injectCss(iconWebview);
      }
    }
  }

  private injectCss(webview: chrome.webviewTag.WebView) {
    webview.addEventListener('contentload', () => {
      webview.insertCSS(
          {
            code: `body { background-color: transparent !important; }`,
          },
          () => {
            if (chrome.runtime.lastError) {
              console.warn(
                  'Failed to insertCSS: ' + chrome.runtime.lastError.message);
            }
          });
    });
  }

  /**
   * Return if categories title should be shown.
   */
  private shouldShowCategoriesTitle(categoriesMapApps: CategoriesAppsMap):
      boolean {
    return !(Object.keys(categoriesMapApps).length > 1);
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


  getApps(key: string): OobeAppsListData {
    return this.categoriesMapApps[key];
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
    return 'cr-checkbox-' + appId;
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
