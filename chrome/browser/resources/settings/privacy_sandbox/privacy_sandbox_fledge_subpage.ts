// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import './privacy_sandbox_interest_item.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';

import type {FledgeState, PrivacySandboxBrowserProxy, PrivacySandboxInterest} from './privacy_sandbox_browser_proxy.js';
import {PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_fledge_subpage.html.js';

// TODO(b/369853368): Remove V2 suffix from variables/code/strings.
export interface SettingsPrivacySandboxFledgeSubpageElement {
  $: {
    fledgeToggle: SettingsToggleButtonElement,
    footerV2: HTMLElement,
  };
}

const maxFledgeSitesCount: number = 15;

const SettingsPrivacySandboxFledgeSubpageElementBase =
    RouteObserverMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsPrivacySandboxFledgeSubpageElement extends
    SettingsPrivacySandboxFledgeSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-fledge-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      sitesList_: {
        type: Array,
        observer: 'onSitesListChanged_',
        value() {
          return [];
        },
      },

      /**
       * Helper list used to display the main sites in the current sites
       * section, above the "See all sites" expand button.
       */
      mainSitesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Helper list used to display the remaining sites in the current sites
       * section that are inside the "See all sites" expandable section.
       */
      remainingSitesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      blockedSitesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used to determine that the Sites list was already fetched and to
       * display the current sites description only after the list is loaded,
       * to avoid displaying first the description for an empty list since the
       * array is empty at first when the page is loaded and switching to the
       * default description once the list is fetched.
       */
      isSitesListLoaded_: {
        type: Boolean,
        value: false,
      },

      isLearnMoreDialogOpen_: {
        type: Boolean,
        value: false,
      },

      seeAllSitesExpanded_: {
        type: Boolean,
        value: false,
        observer: 'onSeeAllSitesExpanded_',
      },

      blockedSitesExpanded_: {
        type: Boolean,
        value: false,
        observer: 'onBlockedSitesExpanded_',
      },
    };
  }

  static get maxFledgeSites() {
    return maxFledgeSitesCount;
  }

  private sitesList_: PrivacySandboxInterest[];
  private mainSitesList_: PrivacySandboxInterest[];
  private remainingSitesList_: PrivacySandboxInterest[];
  private blockedSitesList_: PrivacySandboxInterest[];
  private isSitesListLoaded_: boolean;
  private isLearnMoreDialogOpen_: boolean;
  private seeAllSitesExpanded_: boolean;
  private blockedSitesExpanded_: boolean;
  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getFledgeState().then(
        state => this.onFledgeStateChanged_(state));

    this.$.footerV2.querySelectorAll('a').forEach(
        link =>
            link.setAttribute('aria-description', this.i18n('opensInNewTab')));
  }

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_SANDBOX_FLEDGE) {
      HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
          TrustSafetyInteraction.OPENED_FLEDGE_SUBPAGE);
    }
  }

  private isFledgePrefManaged_(): boolean {
    const fledgeEnabledPref = this.getPref('privacy_sandbox.m1.fledge_enabled');
    if (fledgeEnabledPref.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      assert(!fledgeEnabledPref.value);
      return true;
    }
    return false;
  }

  private onFledgeStateChanged_(state: FledgeState) {
    this.sitesList_ = state.joiningSites.map(site => {
      return {site, removed: false};
    });
    this.blockedSitesList_ = state.blockedSites.map(site => {
      return {site, removed: true};
    });
    this.isSitesListLoaded_ = true;
  }

  private onSitesListChanged_() {
    this.mainSitesList_ = this.sitesList_.slice(0, maxFledgeSitesCount);
    this.remainingSitesList_ = this.sitesList_.slice(maxFledgeSitesCount);
  }

  private isFledgeEnabledAndLoaded_(): boolean {
    return this.getPref('privacy_sandbox.m1.fledge_enabled').value &&
        this.isSitesListLoaded_;
  }

  private isSitesListEmpty_(): boolean {
    return this.sitesList_.length === 0;
  }

  private isRemainingSitesListEmpty_(): boolean {
    return this.remainingSitesList_.length === 0;
  }

  private computeBlockedSitesDescription_(): string {
    return this.i18n(
        this.blockedSitesList_.length === 0 ?
            'fledgePageBlockedSitesDescriptionEmpty' :
            'fledgePageBlockedSitesDescription');
  }

  private getBlockedSitesDescriptionClass_(): string {
    const defaultClass = 'cr-row continuation cr-secondary-text';
    return this.blockedSitesList_.length === 0 ?
        `${defaultClass} no-blocked-sites` :
        defaultClass;
  }

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordAction(
        target.checked ? 'Settings.PrivacySandbox.Fledge.Enabled' :
                         'Settings.PrivacySandbox.Fledge.Disabled');

    // Reset the list after the toggle changed. From disabled -> enabled, the
    // list should already be empty. From enabled -> disabled, the current list
    // is cleared.
    this.sitesList_ = [];
  }

  private onLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Fledge.LearnMoreClicked');
    this.isLearnMoreDialogOpen_ = true;
  }

  private onCloseDialog_() {
    this.isLearnMoreDialogOpen_ = false;
    afterNextRender(this, async () => {
      // `learnMoreLink` might be null if the toggle was disabled after the
      // dialog was opened.
      this.shadowRoot!.querySelector<HTMLElement>('#learnMoreLink')?.focus();
    });
  }

  private onInterestChanged_(e: CustomEvent<PrivacySandboxInterest>) {
    const interest = e.detail;
    assert(!interest.topic);
    if (interest.removed) {
      this.blockedSitesList_.splice(
          this.blockedSitesList_.indexOf(interest), 1);
    } else {
      this.sitesList_.splice(this.sitesList_.indexOf(interest), 1);
      // Move the removed site automatically to the removed section.
      this.blockedSitesList_.push({site: interest.site, removed: true});
      this.blockedSitesList_.sort(
          (first, second) => (first.site! < second.site!) ? -1 : 1);
    }
    this.sitesList_ = this.sitesList_.slice();
    this.blockedSitesList_ = this.blockedSitesList_.slice();
    // If the interest was previously removed, set it to allowed, and vice
    // versa.
    this.privacySandboxBrowserProxy_.setFledgeJoiningAllowed(
        interest.site!, /*allowed=*/ interest.removed);

    this.metricsBrowserProxy_.recordAction(
        interest.removed ? 'Settings.PrivacySandbox.Fledge.SiteAdded' :
                           'Settings.PrivacySandbox.Fledge.SiteRemoved');

    // After allowing or blocking the last item, the focus is lost after the
    // item is removed. Set the focus to the #blockedSitesRow element.
    afterNextRender(this, async () => {
      if (!this.shadowRoot!.activeElement) {
        this.shadowRoot!.querySelector<HTMLElement>('#blockedSitesRow')
            ?.focus();
      }
    });
  }

  private onSeeAllSitesExpanded_() {
    if (this.seeAllSitesExpanded_) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Fledge.AllSitesOpened');
    }
  }

  private onBlockedSitesExpanded_() {
    if (this.blockedSitesExpanded_) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Fledge.BlockedSitesOpened');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-fledge-subpage':
        SettingsPrivacySandboxFledgeSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxFledgeSubpageElement.is,
    SettingsPrivacySandboxFledgeSubpageElement);
