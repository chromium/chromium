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
import {getTemplate} from './internals_page.html.js';
import type {PrivacySandboxInternalsPrefPageConfig} from './pref_page.js';
import type {PrivacySandboxInternalsPref} from './privacy_sandbox_internals.mojom-webui.js';
import {PrivacySandboxInternalsBrowserProxy} from './privacy_sandbox_internals_browser_proxy.js';
import {Router} from './router.js';
import type {RouteObserver} from './router.js';

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
    prefGroups: [
      {
        id: 'advertising',
        title: 'Advertising Prefs',
        prefPrefixes: advertisingPrefPrefixes,
      },
    ],
  },
];

export class InternalsPage extends CustomElement implements RouteObserver {
  private browserProxy_: PrivacySandboxInternalsBrowserProxy =
      PrivacySandboxInternalsBrowserProxy.getInstance();
  private tabBox_: HTMLElement|null = null;
  private panels_: NodeListOf<HTMLElement>;
  static get is() {
    return 'internals-page';
  }

  constructor() {
    super();
    this.panels_ = this.shadowRoot!.querySelectorAll('.panel');
    Router.getInstance().addObserver(this);
  }

  connectedCallback() {
    this.setupEventListeners();
    this.load();
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

  // This accepts a reference to an HTMLElement and a list of prefixes and it
  // renders matching prefs from prefs in the HTMLElement.
  maybeAddPrefsToDom(
      parentElement: HTMLElement|null, prefixes: string[],
      prefs: PrivacySandboxInternalsPref[]) {
    if (parentElement) {
      const filteredPrefs = prefs.filter(
          (pref) => prefixes.some((prefix) => pref.name.startsWith(prefix)));
      this.addPrefsToDom(parentElement, filteredPrefs);
    } else {
      console.error('Parent element not defined for prefixList:', prefixes);
    }
  }
  // Accepts a list prefs and displays them in the parent element
  addPrefsToDom(
      parentElement: HTMLElement, prefs: PrivacySandboxInternalsPref[]) {
    prefs.forEach(({name, value}) => {
      const item = document.createElement('pref-display');
      parentElement.appendChild(item);
      item.configure(name, value);
    });
  }
  // Called when the route changes, this method updates the selected tab in the
  // UI to match the current page in the URL.
  onRouteChanged(pageName: string): void {
    const frameList =
        this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page');
    if (!frameList) {
      return;
    }
    const allTabsInDom =
        Array.from(frameList.querySelectorAll<HTMLElement>('[slot="tab"]'));
    let index = allTabsInDom.findIndex(
        (tab: HTMLElement) => tab.dataset['pageName'] === pageName);
    // If no direct match, fall back to the pre-selected tab.
    if (index === -1) {
      index = allTabsInDom.findIndex(
          (tab: HTMLElement) => tab.hasAttribute('selected'));
    }
    // If a match is found, update the UI. Otherwise, do nothing.
    if (index !== -1) {
      frameList.setAttribute('selected-index', index.toString());
    }
    this.panels_.forEach(p => {
      p.hidden = p.dataset['pageName'] !== pageName;
    });
  }
  // Create the DOM elements needed for the pref pages
  createLayoutForPrefPages() {
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
      // Create the tab for this preference page
      const tab = document.createElement('div');
      tab.setAttribute('slot', 'tab');
      tab.textContent = pageConfig.title;
      tab.dataset['pageName'] = pageConfig.id;
      if (pageConfig.id === 'tracking-protection') {
        tab.setAttribute('selected', '');
      }
      this.tabBox.appendChild(tab);
      // Create the corresponding panel for the tab
      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.classList.add('panel');
      panel.dataset['pageName'] = pageConfig.id;
      panel.hidden = pageConfig.id !== 'tracking-protection';
      // Create the inner structure for this panel
      const mainContentWrapper = document.createElement('div');
      // Use a class for styling instead of a dynamic ID and inline styles
      mainContentWrapper.className = 'main-content-wrapper';
      const searchBar = document.createElement('search-bar');
      mainContentWrapper.appendChild(searchBar);
      const panelsContainer = document.createElement('div');
      // Use a class here as well
      panelsContainer.className = 'panels-container';
      // Create the content (headings and divs) inside the container
      pageConfig.prefGroups.forEach(group => {
        const heading = document.createElement('h3');
        heading.textContent = group.title;
        panelsContainer.appendChild(heading);
        const prefsPanelDiv = document.createElement('div');
        // This ID is unique per group, so keeping it is fine
        prefsPanelDiv.id = `${group.id}-prefs-panel`;
        panelsContainer.appendChild(prefsPanelDiv);
      });
      mainContentWrapper.appendChild(panelsContainer);
      panel.appendChild(mainContentWrapper);
      this.tabBox.appendChild(panel);
    });
  }

  // Fetch pref data and populate the pref pages
  loadAndDisplayPrefs() {
    const allPrefGroups =
        prefPagesToCreate.flatMap((prefPage) => prefPage.prefGroups);
    const allPrefPrefixes = [...new Set(
        allPrefGroups.flatMap((prefGroup) => prefGroup.prefPrefixes))];

    this.browserProxy_.handler.readPrefsWithPrefixes(allPrefPrefixes)
        .then(({prefs}) => allPrefGroups.forEach((prefGroup) => {
          this.maybeAddPrefsToDom(
              this.shadowRoot!.querySelector<HTMLElement>(
                  '#' + prefGroup.id + '-prefs-panel'),
              prefGroup.prefPrefixes, prefs);
        }));
  }

  setupEventListeners() {
    this.tabBox.addEventListener('selected-index-change', () => {
      const selectedTab =
          this.tabBox.querySelector<HTMLElement>('[slot="tab"][selected]');

      if (selectedTab?.dataset['pageName']) {
        Router.getInstance().navigateTo(selectedTab.dataset['pageName']);
      }
    });
  }

  async load() {
    this.createLayoutForPrefPages();
    this.loadAndDisplayPrefs();
    const csPanels = new Map<ContentSettingsType, HTMLElement>();
    const handler = this.browserProxy_.handler;
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

    const addContentSetting = (setting: ContentSettingsType) => {
      // Controls the visibility of the TPCD_METADATA_GRANTS tab.
      if (setting === ContentSettingsType.TPCD_METADATA_GRANTS &&
          !shouldShowTpcdMetadataGrants) {
        return;
      }
      if (setting === ContentSettingsType.DEFAULT) {
        return;
      }
      const tab = document.createElement('div');
      tab.innerText = ContentSettingsType[setting];
      tab.setAttribute('slot', 'tab');
      tab.dataset['pageName'] = ContentSettingsType[setting].toLowerCase();
      this.tabBox.appendChild(tab);

      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.classList.add('panel');
      panel.dataset['pageName'] = ContentSettingsType[setting].toLowerCase();
      panel.hidden = true;
      const panelTitle = document.createElement('h2');
      panelTitle.innerText = ContentSettingsType[setting];
      panel.appendChild(panelTitle);

      const contentSettingsContainer = document.createElement('div');
      contentSettingsContainer.classList.add('content-settings');
      panel.appendChild(contentSettingsContainer);
      this.tabBox.appendChild(panel);

      csPanels.set(setting, contentSettingsContainer);
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
        addContentSetting(setting);
        otherSettings.delete(setting);
      });
    });

    if (otherSettings.size > 0) {
      otherSettings.forEach(setting => addContentSetting(setting));
    }

    // Re-query panels to include the dynamically created ones for routing.
    this.panels_ = this.shadowRoot!.querySelectorAll('.panel');
    for (const [setting, panel] of csPanels.entries()) {
      let mojoResponse;
      if (setting === ContentSettingsType.TPCD_METADATA_GRANTS) {
        // Prevents the TPCD Metadata Grants tab from loading and rendering if
        // its flag is disabled.
        if (!shouldShowTpcdMetadataGrants) {
          continue;
        }
        // This one is special and can't be read through readContentSettings().
        mojoResponse = await handler.getTpcdMetadataGrants();
      } else {
        mojoResponse = await handler.readContentSettings(setting);
      }
      mojoResponse.contentSettings.forEach((cs: any) => {
        const item = document.createElement('content-setting-pattern-source');
        panel.appendChild(item);
        item.configure(handler, cs);
        item.setAttribute('collapsed', 'true');
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'internals-page': InternalsPage;
  }
}
customElements.define('internals-page', InternalsPage);
