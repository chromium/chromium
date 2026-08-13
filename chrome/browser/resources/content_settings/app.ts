// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './content_setting_pattern_source.js';
import './cr_frame_list.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import sheet from './app.css' with {type : 'css'};
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './content_settings_internals.mojom-webui.js';
import {ContentSettingsType} from './content_settings_types.mojom-webui.js';
import type {CrFrameListElement} from './cr_frame_list.js';
import {Router} from './router.js';
import type {RouteObserver} from './router.js';

export class AppElement extends CustomElement implements RouteObserver {
  private panels_: NodeListOf<HTMLElement>|null = null;

  // A set to track which pages have had their data loaded to prevent
  // re-fetching.
  private loadedPages_: Set<string> = new Set();

  static get is() {
    return 'app-element';
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
    Router.getInstance().addObserver(this);
  }

  // Creates the static layout first, then processes the URL.
  connectedCallback() {
    this.createInitialLayout();
    this.setupEventListeners();
    const defaultPage =
        this.shadowRoot!.querySelector<HTMLElement>('[slot="tab"][selected]')
            ?.dataset['pageName'] ||
        'cookies';
    Router.getInstance().processInitialRoute(defaultPage);
  }

  get tabBox(): CrFrameListElement {
    return this.getRequiredElement<CrFrameListElement>('#cs-page');
  }

  disconnectedCallback() {
    Router.getInstance().removeObserver(this);
  }

  // Function for updating the visible page and loading its data on demand.
  async onRouteChanged(pageName: string|null): Promise<void> {
    if (!pageName) {
      return;
    }

    const frameList = this.tabBox;
    const allTabsInDom =
        Array.from(frameList.querySelectorAll<HTMLElement>('[slot="tab"]'));
    let index = allTabsInDom.findIndex(
        (tab: HTMLElement) => tab.dataset['pageName'] === pageName);
    if (index === -1) {
      index = allTabsInDom.findIndex(
          (tab: HTMLElement) => tab.hasAttribute('selected'));
    }
    if (index !== -1) {
      frameList.setAttribute('selected-index', index.toString());
    }

    if (this.panels_) {
      const allPanels = Array.from(this.panels_);
      const activePanel =
          allPanels.find(p => p.dataset['pageName'] === pageName);

      allPanels.forEach(p => {
        p.hidden = (p !== activePanel);
      });

      if (activePanel) {
        // If the data for this page hasn't been loaded yet, load it now.
        if (!this.loadedPages_.has(pageName)) {
          await this.loadDataForPage(pageName);
          this.loadedPages_.add(pageName);
        }
      }
    }
  }

  // A router function to determine which data-loading function to call.
  private async loadDataForPage(pageName: string) {
    const settingType = this.getContentSettingsTypeFromName(pageName);
    if (settingType !== undefined) {
      await this.loadContentSettingsData(settingType, pageName);
      return;
    }

    console.warn(`No data loader found for page: ${pageName}`);
  }

  // Helper function to convert a page name string to its enum type.
  private getContentSettingsTypeFromName(name: string): ContentSettingsType
      |undefined {
    const upperCaseName =
        name.toUpperCase() as keyof typeof ContentSettingsType;
    return ContentSettingsType[upperCaseName];
  }

  // Creates the static layout without fetching any data.
  private createInitialLayout() {
    const settings: string[] = [];
    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      if (i !== ContentSettingsType.DEFAULT &&
          ContentSettingsType[i] !== undefined) {
        settings.push(ContentSettingsType[i]);
      }
    }
    settings.sort((a, b) => a.localeCompare(b));
    render(getHtml(settings), this.shadowRoot!);
    this.panels_ = this.shadowRoot!.querySelectorAll('.panel');
  }

  // Fetches and displays data for a *single* content settings page.
  private async loadContentSettingsData(
      setting: ContentSettingsType, pageName: string) {
    const panel = this.shadowRoot!.querySelector<HTMLElement>(
        `.panel[data-page-name="${pageName}"] .content-settings`);
    if (!panel) {
      console.error(`Content settings panel for ${pageName} not found.`);
      return;
    }

    const handler = browserProxyFactory.getInstance().handler;
    const mojoResponse = await handler.readContentSettings(setting);

    for (const cs of mojoResponse.contentSettings) {
      const item = document.createElement('content-setting-pattern-source');
      panel.appendChild(item);
      await item.configure(handler, cs);
    }
  }

  private setupEventListeners() {
    this.tabBox.addEventListener('selected-index-change', () => {
      const selectedTab =
          this.tabBox.querySelector<HTMLElement>('[slot="tab"][selected]');
      if (selectedTab?.dataset['pageName']) {
        Router.getInstance().navigateTo(selectedTab.dataset['pageName']);
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-element': AppElement;
  }
}
customElements.define('app-element', AppElement);
