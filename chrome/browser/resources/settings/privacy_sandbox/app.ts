// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './icons.html.js';
import './interest_item.js';
import '../settings.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Those resources are loaded through settings.js as the privacy sandbox page
// lives outside regular settings, hence can't access those resources directly
// with |optimize_webui="true"|.
import {CrSettingsPrefs, FledgeState, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxy, MetricsBrowserProxyImpl, PrefsMixin, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl, PrivacySandboxInterest, SettingsToggleButtonElement, TooltipMixin, TopicsState, TrustSafetyInteraction} from '../settings.js';

import {getTemplate} from './app.html.js';

/** Views of the PrivacySandboxSettings page. */
export enum PrivacySandboxSettingsView {
  MAIN = 'main',
  LEARN_MORE_DIALOG = 'learnMoreDialog',
  AD_PERSONALIZATION_DIALOG = 'adPersonalizationDialog',
  AD_PERSONALIZATION_REMOVED_DIALOG = 'adPersonalizationRemovedDialog',
  AD_MEASUREMENT_DIALOG = 'adMeasurementDialog',
  SPAM_AND_FRAUD_DIALOG = 'spamAndFraudDialog',
}

export interface PrivacySandboxAppElement {
  $: {
    learnMoreLink: HTMLElement,
    adPersonalizationRow: HTMLElement,
    adMeasurementRow: HTMLElement,
    spamAndFraudRow: HTMLElement,
  };
}

const PrivacySandboxAppElementBase = TooltipMixin(PrefsMixin(PolymerElement));

export class PrivacySandboxAppElement extends PrivacySandboxAppElementBase {
  static get is() {
    return 'privacy-sandbox-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Valid privacy sandbox settings view states. */
      privacySandboxSettingsViewEnum_: {
        type: Object,
        value: PrivacySandboxSettingsView,
      },

      /** The current view. */
      privacySandboxSettingsView: {
        type: String,
        value: PrivacySandboxSettingsView.MAIN,
      },

      /**
       * The topTopics_, blockedTopics_, joiningSites_, and blockedSites_
       * arrays are used as models to keep the UI in sync with the backend's
       * expected Topics/Fledge states, while the user is on the page.
       */
      topTopics_: {
        type: Array,
        value() {
          return [];
        },
      },

      blockedTopics_: {
        type: Array,
        value() {
          return [];
        },
      },

      joiningSites_: {
        type: Array,
        value() {
          return [];
        },
      },

      blockedSites_: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  privacySandboxSettingsView: PrivacySandboxSettingsView;
  private topTopics_: PrivacySandboxInterest[];
  private blockedTopics_: PrivacySandboxInterest[];
  private joiningSites_: PrivacySandboxInterest[];
  private blockedSites_: PrivacySandboxInterest[];

  override ready() {
    super.ready();
    assert(!loadTimeData.getBoolean('isPrivacySandboxRestricted'));

    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        'WebUI.Settings.PathVisited', '/privacySandbox');

    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));
    this.privacySandboxBrowserProxy_.getFledgeState().then(
        state => this.onFledgeStateChanged_(state));

    // Make the required policy strings available at the window level. This is
    // expected by cr-elements related to policy.
    window.CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingExtensionWithoutName:
          loadTimeData.getString('controlledSettingExtensionWithoutName'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
    };

    CrSettingsPrefs.initialized.then(() => {
      // Wait for preferences to be initialized before writing.
      this.setPrefValue('privacy_sandbox.page_viewed', true);

      // Opening a subpage may result in attempted pref access.
      const view = new URLSearchParams(window.location.search).get('view');
      if (Object.values(PrivacySandboxSettingsView)
              .includes(view as PrivacySandboxSettingsView)) {
        this.privacySandboxSettingsView = view as PrivacySandboxSettingsView;
      } else {
        // If no view has been specified, then navigate to main page.
        this.privacySandboxSettingsView = PrivacySandboxSettingsView.MAIN;
      }
    });

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_PRIVACY_SANDBOX);
  }

  private onApiToggleButtonChange_(event: Event) {
    const privacySandboxApisEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        privacySandboxApisEnabled ? 'Settings.PrivacySandbox.ApisEnabled' :
                                    'Settings.PrivacySandbox.ApisDisabled');
    this.setPrefValue('privacy_sandbox.manually_controlled_v2', true);

    // As the backend will have cleared any data when the API is disabled, clear
    // the associated model entries.
    if (!privacySandboxApisEnabled) {
      this.topTopics_ = [];
      this.joiningSites_ = [];
    }
  }

  private showFragment_(view: PrivacySandboxSettingsView): boolean {
    return this.privacySandboxSettingsView === view;
  }

  private onDialogClose_() {
    // This function will only be called once, regardless of how the dialog is
    // shut (either via ESC or via the button), as in the latter the dialog is
    // not "closed", but rather removed from the DOM.
    const lastView = this.privacySandboxSettingsView;
    this.privacySandboxSettingsView = PrivacySandboxSettingsView.MAIN;
    afterNextRender(this, async () => {
      switch (lastView) {
        case PrivacySandboxSettingsView.LEARN_MORE_DIALOG:
          this.$.learnMoreLink.focus();
          break;
        case PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG:
        case PrivacySandboxSettingsView.AD_PERSONALIZATION_REMOVED_DIALOG:
          this.$.adPersonalizationRow.focus();
          break;
        case PrivacySandboxSettingsView.AD_MEASUREMENT_DIALOG:
          this.$.adMeasurementRow.focus();
          break;
        case PrivacySandboxSettingsView.SPAM_AND_FRAUD_DIALOG:
          this.$.spamAndFraudRow.focus();
          break;
      }
    });
  }

  private onLearnMoreClick_(e: Event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    e.stopPropagation();
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdPersonalization.LearnMoreClicked');
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.LEARN_MORE_DIALOG;
  }

  private onAdPersonalizationRowClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdPersonalization.Opened');
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG;
  }

  private getAdPersonalizationDialogDescription_(): string {
    const enabled = this.getPref('privacy_sandbox.apis_enabled_v2').value;
    if (enabled) {
      return loadTimeData.getString(
          this.topTopics_.length || this.blockedTopics_.length ||
                  this.joiningSites_.length || this.blockedSites_.length ?
              'privacySandboxAdPersonalizationDialogDescription' :
              'privacySandboxAdPersonalizationDialogDescriptionListsEmpty');
    }
    return loadTimeData.getString(
        'privacySandboxAdPersonalizationDialogDescriptionTrialsOff');
  }

  private onAdPersonalizationRemovedRowClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.RemovedInterests.Opened');
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.AD_PERSONALIZATION_REMOVED_DIALOG;
  }

  private onAdPersonalizationBackButtonClick_() {
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG;
  }

  private onAdMeasurementRowClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdMeasurement.Opened');
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.AD_MEASUREMENT_DIALOG;
  }

  private getAdMeasurementDialogDescription_(): string {
    const enabled = this.getPref('privacy_sandbox.apis_enabled_v2').value;
    return loadTimeData.getString(
        enabled ? 'privacySandboxAdMeasurementDialogDescription' :
                  'privacySandboxAdMeasurementDialogDescriptionTrialsOff');
  }

  private onSpamAndFraudRowClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.SpamFraud.Opened');
    this.privacySandboxSettingsView =
        PrivacySandboxSettingsView.SPAM_AND_FRAUD_DIALOG;
  }

  private getSpamAndFraudDialogDescription1_(): string {
    const enabled = this.getPref('privacy_sandbox.apis_enabled_v2').value;
    return loadTimeData.getString(
        enabled ? 'privacySandboxSpamAndFraudDialogDescription1' :
                  'privacySandboxSpamAndFraudDialogDescription1TrialsOff');
  }

  private showInterestsList_(interests: PrivacySandboxInterest[]): boolean {
    return interests.length > 0;
  }

  private onTopicsStateChanged_(state: TopicsState) {
    this.topTopics_ = state.topTopics.map(topic => {
      return {topic, removed: false};
    });
    this.blockedTopics_ = state.blockedTopics.map(topic => {
      return {topic, removed: true};
    });
  }

  private onTopicInteracted_(interest: PrivacySandboxInterest) {
    assert(!interest.site);
    if (interest.removed) {
      this.blockedTopics_.splice(this.blockedTopics_.indexOf(interest), 1);
    } else {
      this.topTopics_.splice(this.topTopics_.indexOf(interest), 1);
      // Move the removed topic automatically to the removed section.
      this.blockedTopics_.push({topic: interest.topic, removed: true});
      this.blockedTopics_.sort(
          (first, second) =>
              first.topic!.displayString < second.topic!.displayString ? -1 :
                                                                         1);
    }
    // This causes the lists to be fully re-rendered, in order to reflect
    // the models' changes.
    this.topTopics_ = this.topTopics_.slice();
    this.blockedTopics_ = this.blockedTopics_.slice();
    // If the interest was previously removed, set it to allowed, and vice
    // versa.
    this.metricsBrowserProxy_.recordAction(
        interest.removed ?
            'Settings.PrivacySandbox.RemovedInterests.TopicAdded' :
            'Settings.PrivacySandbox.AdPersonalization.TopicRemoved');
    this.privacySandboxBrowserProxy_.setTopicAllowed(
        interest.topic!, /*allowed=*/ interest.removed);
  }

  private onFledgeStateChanged_(state: FledgeState) {
    this.joiningSites_ = state.joiningSites.map(site => {
      return {site, removed: false};
    });
    this.blockedSites_ = state.blockedSites.map(site => {
      return {site, removed: true};
    });
  }

  private onSiteInteracted_(interest: PrivacySandboxInterest) {
    assert(!interest.topic);
    if (interest.removed) {
      this.blockedSites_.splice(this.blockedSites_.indexOf(interest), 1);
    } else {
      this.joiningSites_.splice(this.joiningSites_.indexOf(interest), 1);
      // Move the removed site automatically to the removed section.
      this.blockedSites_.push({site: interest.site, removed: true});
      this.blockedSites_.sort(
          (first, second) => first.site! < second.site!? -1 : 1);
    }
    this.joiningSites_ = this.joiningSites_.slice();
    this.blockedSites_ = this.blockedSites_.slice();
    // If the interest was previously removed, set it to allowed, and vice
    // versa.
    this.metricsBrowserProxy_.recordAction(
        interest.removed ?
            'Settings.PrivacySandbox.RemovedInterests.SiteAdded' :
            'Settings.PrivacySandbox.AdPersonalization.SiteRemoved');
    this.privacySandboxBrowserProxy_.setFledgeJoiningAllowed(
        interest.site!, /*allowed=*/ interest.removed);
  }

  private onInterestChanged_(e: CustomEvent<PrivacySandboxInterest>) {
    const interest = e.detail;
    if (interest.topic !== undefined) {
      this.onTopicInteracted_(interest);
    } else {
      this.onSiteInteracted_(interest);
    }
  }

  private onShowTooltip_(e: Event) {
    assert(e.target instanceof HTMLElement);
    const target = e.target! as HTMLElement;
    const tooltip = this.shadowRoot!.querySelector<PaperTooltipElement>(
        target.id === 'topicsTooltipIcon' ? '#topicsTooltip' :
                                            '#fledgeTooltip')!;
    this.showTooltipAtTarget(tooltip, target);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-app': PrivacySandboxAppElement;
  }
}

customElements.define(PrivacySandboxAppElement.is, PrivacySandboxAppElement);
