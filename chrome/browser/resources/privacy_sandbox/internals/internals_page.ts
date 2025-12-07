// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './content_setting_pattern_source.js';
import './cr_frame_list.js';
import './mojo_timedelta.js';
import './pref_display.js';
import './pref_page.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './privacy_sandbox_internals.mojom-webui.js';
import './search_bar.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {contentSettingGroups} from './content_settings_groups.js';
import {ContentSettingsType} from './content_settings_types.mojom-webui.js';
import type {CrFrameListElement} from './cr_frame_list.js';
import {highlight, unhighlight} from './highlight_utils.js';
import {getTemplate} from './internals_page.html.js';
import type {PrivacySandboxInternalsPrefPageConfig} from './pref_page.js';
import type {PrivacySandboxInternalsPref} from './privacy_sandbox_internals.mojom-webui.js';
import {PrivacySandboxInternalsBrowserProxy} from './privacy_sandbox_internals_browser_proxy.js';
import {Router} from './router.js';
import type {RouteObserver} from './router.js';
import type {SearchBarElement} from './search_bar.js';

// Caching interfaces
interface CachedItem {
  element: HTMLElement;
  content: string;
}

const tpcdExperimentPrefPrefixes: string[] = [
  'tpcd_experiment.',
  'uninstall_metrics.installation_date2',
  'profile.cookie_controls_mode',
  'profile.cookie_block_truncated',
];

const trackingProtectionPrefPrefixes: string[] = [
  'profile.managed_cookies_allowed_for_urls',
  'enable_do_not_track',
  'tracking_protection.',
];

const advertisingPrefPrefixes: string[] = [
  'privacy_sandbox.',
];

const prefPagesToCreate: PrivacySandboxInternalsPrefPageConfig[] = [
  {
    id: 'tracking-protection',
    title: 'Tracking Protection / 3PCD Prefs',
    prefGroups: [
      {
        id: 'tracking-protection',
        title: 'Tracking Protection Service Prefs',
        prefPrefixes: trackingProtectionPrefPrefixes,
      },
      {
        id: 'tpcd-experiment',
        title: '3PCD Experiment Prefs',
        prefPrefixes: tpcdExperimentPrefPrefixes,
      },
    ],
  },
  {
    id: 'advertising',
    title: 'Advertising Prefs',
    prefGroups: [{
      id: 'advertising',
      title: 'Advertising Prefs',
      prefPrefixes: advertisingPrefPrefixes,
    }],
  },
];

export class InternalsPage extends CustomElement implements RouteObserver {
  private browserProxy_: PrivacySandboxInternalsBrowserProxy =
      PrivacySandboxInternalsBrowserProxy.getInstance();
  private tabBox_: HTMLElement|null = null;
  private panels_: NodeListOf<HTMLElement> =
      this.shadowRoot!.querySelectorAll('.panel');
  private activePageName_: string|null = null;

  // Caching arrays
  private cachedItems_: Map<string, CachedItem[]> = new Map();

  // A set to track which pages have had their data loaded to prevent
  // re-fetching.
  private loadedPages_: Set<string> = new Set();

  static get is() {
    return 'internals-page';
  }

  constructor() {
    super();
    Router.getInstance().addObserver(this);
  }

  // Creates the static layout first, then processes the URL.
  connectedCallback() {
    this.createInitialLayout();
    this.setupEventListeners();
    const defaultPage =
        this.shadowRoot!.querySelector<HTMLElement>('[slot="tab"][selected]')
            ?.dataset['pageName']!;
    Router.getInstance().processInitialRoute(defaultPage);
  }

  get tabBox(): HTMLElement {
    if (!this.tabBox_) {
      this.tabBox_ =
          this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page')!;
    }
    return this.tabBox_;
  }

  static override get template() {
    return getTemplate();
  }

  disconnectedCallback() {
    Router.getInstance().removeObserver(this);
  }

  // Function for updating the visible page and loading its data on demand.
  async onRouteChanged(pageName: string|null, query: string|null):
      Promise<void> {
    if (!pageName) {
      return;
    }
    this.activePageName_ = pageName;

    const frameList =
        this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page')!;
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

    const allPanels = Array.from(this.panels_);
    const activePanel = allPanels.find(p => p.dataset['pageName'] === pageName);

    allPanels.forEach(p => {
      p.hidden = (p !== activePanel);
    });

    if (activePanel) {
      // If the data for this page hasn't been loaded yet, load it now.
      if (!this.loadedPages_.has(pageName)) {
        await this.loadDataForPage(pageName);
        this.loadedPages_.add(pageName);
      }

      // Now that data is guaranteed to be loaded, proceed with filtering.
      if (activePanel) {
        // If the data for this page hasn't been loaded yet, load it now.
        if (!this.loadedPages_.has(pageName)) {
          await this.loadDataForPage(pageName);
          this.loadedPages_.add(pageName);
        }

        // Now that data is guaranteed to be loaded, proceed with filtering.
        const searchBar =
            activePanel.querySelector<SearchBarElement>('search-bar');
        if (searchBar) {
          searchBar.setQuery(query || '');
          this.filterAndHighlightContent(query);
          if (query) {
            searchBar.focusInput();
          }
        }
      }
    }
  }

  // A router function to determine which data-loading function to call.
  private async loadDataForPage(pageName: string) {
    const prefPageConfig = prefPagesToCreate.find(p => p.id === pageName);
    if (prefPageConfig) {
      await this.loadPrefsForPage(prefPageConfig);
      return;
    }

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
    const upperCaseName = name.toUpperCase();
    return ContentSettingsType[upperCaseName as keyof typeof ContentSettingsType];
  }

  // Filters items by visibility and applies highlighting.
  private filterAndHighlightContent(query: string|null) {
    const lowerCaseQuery = query ? query.toLowerCase().trim() : '';
    const activeItems = this.cachedItems_.get(this.activePageName_!) || [];

    for (const item of activeItems) {
      const isMatch = !lowerCaseQuery || item.content.includes(lowerCaseQuery);

      // First, remove any previous highlights from this item.
      unhighlight(item.element);

      // Determine if the item's content matches the search query.
      // Hide the element if it's not a match.
      item.element.hidden = !isMatch;

      // If it's a match and there's a search query, apply highlighting.
      if (isMatch && lowerCaseQuery) {
        highlight(item.element, lowerCaseQuery);
      }
    }
  }

  // Creates the static layout without fetching any data.
  private createInitialLayout() {
    this.createPrefPageLayout();
    this.createContentSettingsPageLayout();
    this.panels_ = this.shadowRoot!.querySelectorAll('.panel');
  }

  // Creates the DOM elements needed for the pref pages but does not populate
  // them.
  private createPrefPageLayout() {
    const headerTab = document.createElement('div');
    headerTab.innerText = 'Prefs';
    headerTab.className = 'settings-category-header';
    headerTab.setAttribute('role', 'heading');
    headerTab.setAttribute('slot', 'tab');
    this.tabBox.appendChild(headerTab);
    const headerPanel = document.createElement('div');
    headerPanel.setAttribute('slot', 'panel');
    this.tabBox.appendChild(headerPanel);

    prefPagesToCreate.forEach((pageConfig) => {
      const tab = document.createElement('div');
      tab.setAttribute('slot', 'tab');
      tab.textContent = pageConfig.title;
      tab.dataset['pageName'] = pageConfig.id;
      if (pageConfig.id === 'tracking-protection') {
        tab.setAttribute('selected', '');
      }
      this.tabBox.appendChild(tab);

      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.classList.add('panel');
      panel.dataset['pageName'] = pageConfig.id;
      panel.hidden = pageConfig.id !== 'tracking-protection';

      const mainContentWrapper = document.createElement('div');
      mainContentWrapper.className = 'main-content-wrapper';

      // Wrap the search-bar in a new container
      const searchContainer = document.createElement('div');
      searchContainer.className = 'search-bar-container';
      searchContainer.appendChild(document.createElement('search-bar'));
      mainContentWrapper.appendChild(searchContainer);

      const panelsContainer = document.createElement('div');
      panelsContainer.className = 'panels-container';

      pageConfig.prefGroups.forEach(group => {
        const heading = document.createElement('h3');
        heading.textContent = group.title;
        panelsContainer.appendChild(heading);
        const prefsPanelDiv = document.createElement('div');
        prefsPanelDiv.id = `${group.id}-prefs-panel`;
        panelsContainer.appendChild(prefsPanelDiv);
      });

      mainContentWrapper.appendChild(panelsContainer);
      panel.appendChild(mainContentWrapper);
      this.tabBox.appendChild(panel);
    });
  }

  // Fetches and displays prefs for a *single* pref page configuration.
  private async loadPrefsForPage(
      pageConfig: PrivacySandboxInternalsPrefPageConfig) {
    const allPrefPrefixesForPage =
        pageConfig.prefGroups.flatMap((prefGroup) => prefGroup.prefPrefixes);
    const uniquePrefixes = [...new Set(allPrefPrefixesForPage)];

    const {prefs} =
        await this.browserProxy_.handler.readPrefsWithPrefixes(uniquePrefixes);

    pageConfig.prefGroups.forEach((prefGroup) => {
      this.addPrefsToDom(
          this.shadowRoot!.querySelector<HTMLElement>(
              '#' + prefGroup.id + '-prefs-panel'),
          prefs.filter(
              (pref) => prefGroup.prefPrefixes.some(
                  (prefix) => pref.name.startsWith(prefix))),
          pageConfig.id);
    });
  }

  private addPrefsToDom(
      parentElement: HTMLElement|null, prefs: PrivacySandboxInternalsPref[],
      pageName: string) {
    if (!parentElement) {
      console.error('Parent element not found for pref group.');
      return;
    }

    if (!this.cachedItems_.has(pageName)) {
      this.cachedItems_.set(pageName, []);
    }
    const pageItems = this.cachedItems_.get(pageName)!;

    prefs.forEach(({name, value}) => {
      const item = document.createElement('pref-display');
      item.configure(name, value);
      parentElement.appendChild(item);
      const contentString = `${name} ${JSON.stringify(value)}`.toLowerCase();
      pageItems.push({
        element: item,
        content: contentString,
      });
    });
  }

  // Creates the layout for content settings pages without populating them.
  private createContentSettingsPageLayout() {
    const shouldShowTpcdMetadataGrants =
        this.browserProxy_.shouldShowTpcdMetadataGrants();

    const addHeaderToTabBox = (name: string, className: string) => {
      const headerTab = document.createElement('div');
      headerTab.innerText = name;
      headerTab.className = className;
      headerTab.setAttribute('role', 'heading');
      headerTab.setAttribute('slot', 'tab');
      this.tabBox.appendChild(headerTab);
      const headerPanel = document.createElement('div');
      headerPanel.setAttribute('slot', 'panel');
      this.tabBox.appendChild(headerPanel);
    };

    const addContentSettingPanel = (setting: ContentSettingsType) => {
      // Controls the visibility of the TPCD_METADATA_GRANTS tab.
      if (setting === ContentSettingsType.TPCD_METADATA_GRANTS &&
          !shouldShowTpcdMetadataGrants) {
        return;
      }
      if (setting === ContentSettingsType.DEFAULT) {
        return;
      }
      const pageName = ContentSettingsType[setting].toLowerCase();

      const tab = document.createElement('div');
      tab.innerText = ContentSettingsType[setting];
      tab.setAttribute('slot', 'tab');
      tab.dataset['pageName'] = pageName;
      this.tabBox.appendChild(tab);

      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.classList.add('panel');
      panel.dataset['pageName'] = pageName;
      panel.hidden = true;

      const mainContentWrapper = document.createElement('div');
      mainContentWrapper.className = 'main-content-wrapper';

      // Wrap the search-bar in a new container
      const searchContainer = document.createElement('div');
      searchContainer.className = 'search-bar-container';
      searchContainer.appendChild(document.createElement('search-bar'));
      mainContentWrapper.appendChild(searchContainer);

      const panelsContainer = document.createElement('div');
      panelsContainer.className = 'panels-container';
      const panelTitle = document.createElement('h2');
      panelTitle.innerText = ContentSettingsType[setting];
      panelsContainer.appendChild(panelTitle);

      const contentSettingsContainer = document.createElement('div');
      contentSettingsContainer.classList.add('content-settings');
      panelsContainer.appendChild(contentSettingsContainer);

      mainContentWrapper.appendChild(panelsContainer);
      panel.appendChild(mainContentWrapper);
      this.tabBox.appendChild(panel);
    };

    addHeaderToTabBox('Content Settings', 'settings-category-header');

    const otherSettings = new Set<ContentSettingsType>();
    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      if (i !== ContentSettingsType.DEFAULT) {
        otherSettings.add(i);
      }
    }

    contentSettingGroups.forEach(group => {
      addHeaderToTabBox(group.name, 'setting-header');
      group.settings.forEach(setting => {
        addContentSettingPanel(setting);
        otherSettings.delete(setting);
      });
    });

    otherSettings.forEach(setting => addContentSettingPanel(setting));
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

    const handler = this.browserProxy_.handler;
    const shouldShowTpcdMetadataGrants =
        this.browserProxy_.shouldShowTpcdMetadataGrants();
    let mojoResponse;

    if (setting === ContentSettingsType.TPCD_METADATA_GRANTS) {
      if (!shouldShowTpcdMetadataGrants) {
        return;
      }
      mojoResponse = await handler.getTpcdMetadataGrants();
    } else {
      mojoResponse = await handler.readContentSettings(setting);
    }

    if (!this.cachedItems_.has(pageName)) {
      this.cachedItems_.set(pageName, []);
    }
    const pageItems = this.cachedItems_.get(pageName)!;

    for (const cs of mojoResponse.contentSettings) {
      const item = document.createElement('content-setting-pattern-source');
      panel.appendChild(item);
      const content = await item.configure(handler, cs);

      pageItems.push({
        element: item,
        content: content.toLowerCase(),
      });
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
    'internals-page': InternalsPage;
  }
}
customElements.define('internals-page', InternalsPage);
