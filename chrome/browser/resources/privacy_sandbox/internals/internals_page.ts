// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './content_setting_pattern_source.js';
import './pref_display.js';
import './mojo_timedelta.js';
import './cr_frame_list.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './privacy_sandbox_internals.mojom-webui.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {ContentSettingsType} from './content_settings_types.mojom-webui.js';
import type {CrFrameListElement} from './cr_frame_list.js';
import {getTemplate} from './internals_page.html.js';
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


export class InternalsPage extends CustomElement implements RouteObserver {
  private browserProxy_: PrivacySandboxInternalsBrowserProxy =
      PrivacySandboxInternalsBrowserProxy.getInstance();
  whenLoaded: Promise<void>|null = null;

  static get is() {
    return 'internals-page';
  }

  constructor() {
    super();
    Router.getInstance().addObserver(this);
  }

  connectedCallback() {
    this.whenLoaded = this.load();
    const defaultPage =
        this.shadowRoot!.querySelector<HTMLElement>('[slot="tab"][selected]')
            ?.dataset['pageName']!;
    Router.getInstance().processInitialRoute(defaultPage);
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
    const index = allTabsInDom.findIndex(
        (tab: HTMLElement) => tab.dataset['pageName'] === pageName);

    if (index !== -1) {
      frameList.setAttribute('selected-index', index.toString());
    } else {
      frameList.setAttribute('selected-index', '0');
    }
  }

  async load() {
    const tabBox =
        this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page')!;

    // We set up the event listener on the #ps-page element at the top of load,
    // before any await statements, otherwise, the registration of the event
    // listener will be delayed till awaited async operations are completed.
    tabBox.addEventListener('selected-index-change', () => {
      const selectedTab =
          tabBox.querySelector<HTMLElement>('[slot="tab"][selected]');

      if (selectedTab?.dataset['pageName']) {
        Router.getInstance().navigateTo(selectedTab.dataset['pageName']);
      }
    });

    const {prefs} = await this.browserProxy_.handler.readPrefsWithPrefixes([
      ...advertisingPrefPrefixes,
      ...trackingProtectionPrefPrefixes,
      ...tpcdExperimentPrefPrefixes,
    ]);

    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>('#advertising-prefs-panel'),
        advertisingPrefPrefixes, prefs);
    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>(
            '#tracking-protection-prefs-panel'),
        trackingProtectionPrefPrefixes, prefs);
    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>(
            '#tpcd-experiment-prefs-panel'),
        tpcdExperimentPrefPrefixes, prefs);

    const csPanels = new Map<string, HTMLElement>();
    const handler = this.browserProxy_.handler;
    const shouldShowTpcdMetadataGrants =
        this.browserProxy_.shouldShowTpcdMetadataGrants();

    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      // Controls the visibility of the TPCD_METADATA_GRANTS tab.
      if (ContentSettingsType[i] === 'TPCD_METADATA_GRANTS' &&
          !shouldShowTpcdMetadataGrants) {
        continue;
      }
      const tab = document.createElement('div');
      tab.innerText = ContentSettingsType[i];
      tab.setAttribute('slot', 'tab');
      tab.dataset['pageName'] = ContentSettingsType[i].toLowerCase();
      tabBox.appendChild(tab);

      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.setAttribute('style', 'content-settings');
      panel.setAttribute('title', ContentSettingsType[i]);
      const panelTitle = document.createElement('h2');
      panelTitle.innerText = ContentSettingsType[i];
      panel.appendChild(panelTitle);
      tabBox.appendChild(panel);

      csPanels.set(ContentSettingsType[i], panel);
    }


    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      let mojoResponse;
      if (i === ContentSettingsType.TPCD_METADATA_GRANTS) {
        // Prevents the TPCD Metadata Grants tab from loading and rendering if
        // its flag is disabled.
        if (!shouldShowTpcdMetadataGrants) {
          continue;
        }
        // This one is special and can't be read through readContentSettings().
        mojoResponse = await handler.getTpcdMetadataGrants();
      } else {
        mojoResponse = await handler.readContentSettings(i);
      }
      mojoResponse.contentSettings.forEach((cs: any) => {
        const panel = csPanels.get(ContentSettingsType[i])!;
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
