// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import './privacy_sandbox_interest_item.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {FocusConfig} from '../focus_config.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import type {CanonicalTopic, PrivacySandboxBrowserProxy, PrivacySandboxInterest, TopicsState} from './privacy_sandbox_browser_proxy.js';
import {PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_topics_subpage.html.js';

export interface SettingsPrivacySandboxTopicsSubpageElement {
  $: {
    topicsToggle: SettingsToggleButtonElement,
    footer: HTMLElement,
    footerV2: HTMLElement,
  };
}

const SettingsPrivacySandboxTopicsSubpageElementBase =
    RouteObserverMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsPrivacySandboxTopicsSubpageElement extends
    SettingsPrivacySandboxTopicsSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-topics-subpage';
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

      topicsList_: {
        type: Array,
        value() {
          return [];
        },
      },

      blockedTopicsList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used to determine that the Topics list was already fetched and to
       * display the current topics description only after the list is loaded,
       * to avoid displaying first the description for an empty list since the
       * array is empty at first when the page is loaded and switching to the
       * default description once the list is fetched.
       */
      isTopicsListLoaded_: {
        type: Boolean,
        value: false,
      },

      isLearnMoreDialogOpen_: {
        type: Boolean,
        value: false,
      },

      blockedTopicsExpanded_: {
        type: Boolean,
        value: false,
        observer: 'onBlockedTopicsExpanded_',
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      // Version 2 of Ad Topics Page should be displayed when Proactive Topics
      // Blocking is enabled.
      // TODO(b/370758849): Cleanup Topics Subpage by removing shouldShowV2_.
      shouldShowV2_: {
        type: Boolean,
        value: true,
      },

      blockTopicDialogTitle_: {
        type: String,
        value: '',
      },

      blockTopicDialogBody_: {
        type: String,
        value: '',
      },

      shouldShowBlockTopicDialog_: {
        type: Boolean,
        value: false,
      },

      shouldShowV2EmptyState_: {
        type: Boolean,
        computed: 'computeShouldShowV2EmptyState_(' +
            'shouldShowV2, prefs.privacy_sandbox.m1.topics_enabled.value)',
      },
    };
  }

  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private topicsList_: PrivacySandboxInterest[];
  private blockedTopicsList_: PrivacySandboxInterest[];
  private currentChildTopics_: CanonicalTopic[];
  private currentInterest_?: PrivacySandboxInterest;
  private focusConfig: FocusConfig;

  private isTopicsListLoaded_: boolean;
  private shouldShowV2_: boolean;
  private shouldShowV2EmptyState_: boolean;
  private blockedTopicsExpanded_: boolean;

  private isLearnMoreDialogOpen_: boolean;
  private shouldShowBlockTopicDialog_: boolean;
  private blockTopicDialogTitle_: string;
  private blockTopicDialogBody_: string;

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));

    this.$.footer.querySelectorAll('a').forEach(
        link =>
            link.setAttribute('aria-description', this.i18n('opensInNewTab')));
    this.$.footerV2.querySelectorAll('a').forEach(
        link =>
            link.setAttribute('aria-description', this.i18n('opensInNewTab')));
  }

  // Goal is to not show anything but the toggle and disclaimer when we
  // should show V2 and the pref is false.
  private computeShouldShowV2EmptyState_(): boolean {
    return (
        this.shouldShowV2_ &&
        !this.getPref('privacy_sandbox.m1.topics_enabled').value);
  }

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_SANDBOX_TOPICS) {
      HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
          TrustSafetyInteraction.OPENED_TOPICS_SUBPAGE);
      // Updating the TopicsState because it can be changed by being
      // blocked/unblocked in the Manage Topics Page. Need to keep the data
      // between the two pages up to date.
      if (this.shouldShowV2_) {
        this.privacySandboxBrowserProxy_.getTopicsState().then(
            state => this.onTopicsStateChanged_(state));
      }
    }
  }

  private isTopicsPrefManaged_(): boolean {
    const topicsEnabledPref = this.getPref('privacy_sandbox.m1.topics_enabled');
    if (topicsEnabledPref.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      assert(!topicsEnabledPref.value);
      return true;
    }
    return false;
  }

  private onTopicsStateChanged_(state: TopicsState) {
    this.topicsList_ = state.topTopics.map(topic => {
      return {topic, removed: false};
    });
    this.blockedTopicsList_ = state.blockedTopics.map(topic => {
      return {topic, removed: true};
    });
    this.isTopicsListLoaded_ = true;
  }

  private isTopicsEnabledAndLoaded_(): boolean {
    return this.getPref('privacy_sandbox.m1.topics_enabled').value &&
        this.isTopicsListLoaded_;
  }

  private isTopicsListEmpty_(): boolean {
    return this.topicsList_.length === 0 && !this.shouldShowV2_;
  }

  private isTopicsListEmptyV2_(): boolean {
    return this.topicsList_.length === 0 && this.shouldShowV2_;
  }

  private isBlockedTopicsListEmptyV2_(): boolean {
    return this.blockedTopicsList_.length === 0 && this.shouldShowV2_;
  }

  private computeBlockedTopicsDescription_(): string {
    return this.i18n(
        this.blockedTopicsList_.length === 0 ?
            'topicsPageBlockedTopicsDescriptionEmpty' :
            'topicsPageBlockedTopicsDescription');
  }

  private getBlockedTopicsDescriptionClass_(): string {
    const defaultClass = 'cr-row continuation cr-secondary-text';
    return this.blockedTopicsList_.length === 0 ?
        `${defaultClass} no-blocked-topics` :
        defaultClass;
  }

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordAction(
        target.checked ? 'Settings.PrivacySandbox.Topics.Enabled' :
                         'Settings.PrivacySandbox.Topics.Disabled');

    this.privacySandboxBrowserProxy_.topicsToggleChanged(target.checked);

    // Reset the list after the toggle changed. From disabled -> enabled, the
    // list should already be empty. From enabled -> disabled, the current list
    // is cleared.
    this.topicsList_ = [];
  }

  private onLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Topics.LearnMoreClicked');
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

  private onBlockTopicDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            'settings-simple-confirmation-dialog');
    assert(dialog);
    assert(this.currentInterest_);
    if (dialog.wasConfirmed()) {
      this.updateTopicsStateForSelectedTopic_(this.currentInterest_!);
    }
    this.blockTopicDialogBody_ = '';
    this.blockTopicDialogTitle_ = '';
    this.currentChildTopics_ = [];
    this.shouldShowBlockTopicDialog_ = false;
    this.currentInterest_ = undefined;
  }

  private updateTopicsStateForSelectedTopic_(currentSelectedInterest:
                                                 PrivacySandboxInterest) {
    this.privacySandboxBrowserProxy_.setTopicAllowed(
        currentSelectedInterest.topic!,
        /*allowed=*/ currentSelectedInterest.removed);
    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));

    this.metricsBrowserProxy_.recordAction(
        currentSelectedInterest.removed ?
            'Settings.PrivacySandbox.Topics.TopicAdded' :
            'Settings.PrivacySandbox.Topics.TopicRemoved');

    // After allowing or blocking the last item, the focus is lost after the
    // item is removed. Set the focus to the #blockedTopicsRow element.
    afterNextRender(this, async () => {
      if (!this.shadowRoot!.activeElement) {
        this.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow')
            ?.focus();
      }
    });
  }

  // In the V2 of the ad topics page, this function is invoked by
  // onInterestedChanged_ to handle the blocking/allowing and updating state.
  private async onInterestChangedV2_() {
    assert(this.currentInterest_!.topic);
    assert(this.currentInterest_!.topic!.displayString);

    // If topic is being unblocked, show toast and update topic state.
    if (this.currentInterest_!.removed) {
      const toast = this.shadowRoot!.querySelector('cr-toast');
      assert(toast);
      toast.show();
      this.updateTopicsStateForSelectedTopic_(this.currentInterest_!);
      return;
    }

    this.currentChildTopics_ =
        await this.privacySandboxBrowserProxy_.getChildTopicsCurrentlyAssigned(
            this.currentInterest_!.topic!);
    // Check if currently selected topic to block has active child topics
    // if it does, show simple confirmation dialog.
    if (this.currentChildTopics_.length !== 0) {
      this.blockTopicDialogTitle_ = loadTimeData.getStringF(
          'manageTopicsDialogTitle',
          this.currentInterest_!.topic!.displayString);
      this.blockTopicDialogBody_ = loadTimeData.getStringF(
          'manageTopicsDialogBody',
          this.currentInterest_!.topic!.displayString);
      this.shouldShowBlockTopicDialog_ = true;
      return;
    }
    // Currently selected topic doesn't have active child topics.
    // Update topics state.
    this.updateTopicsStateForSelectedTopic_(this.currentInterest_!);
    this.blockedTopicsExpanded_ = true;
  }

  // This function is run anytime the interest item changes. Which means that it
  // runs when a user blocks/allows a topic.
  private onInterestChanged_(e: CustomEvent<PrivacySandboxInterest>) {
    this.currentInterest_ = e.detail;
    assert(!this.currentInterest_.site);
    if (this.shouldShowV2_) {
      this.onInterestChangedV2_();
      return;
    }
    this.updateTopicsStateForSelectedTopic_(this.currentInterest_!);
  }

  private onBlockedTopicsExpanded_() {
    if (this.blockedTopicsExpanded_) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Topics.BlockedTopicsOpened');
    }
  }

  private onPrivacySandboxManageTopicsClick_() {
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_MANAGE_TOPICS);
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    assert(!oldConfig);
    if (routes.PRIVACY_SANDBOX_MANAGE_TOPICS) {
      this.focusConfig.set(routes.PRIVACY_SANDBOX_MANAGE_TOPICS.path, () => {
        const toFocus = this.shadowRoot!.querySelector<HTMLElement>(
            '#privacySandboxManageTopicsLinkRow');
        assert(toFocus);
        focusWithoutInk(toFocus);
      });
    }
  }

  private onHideToastClick_() {
    const toast = this.shadowRoot!.querySelector('cr-toast');
    assert(toast);
    toast.hide();
  }

  private computeTopicsPageToggleSubLabel_(): string {
    return this.i18n(
        this.shouldShowV2_ ? 'topicsPageToggleSubLabelV2' :
                             'topicsPageToggleSubLabel');
  }

  private computeTopicsPageCurrentTopicsHeading_(): string {
    return this.i18n(
        this.shouldShowV2_ ? 'topicsPageActiveTopicsHeading' :
                             'topicsPageCurrentTopicsHeading');
  }

  private computeTopicsPageCurrentTopicsDescription_(): string {
    return this.i18n(
        this.shouldShowV2_ ? 'topicsPageActiveTopicsDescription' :
                             'topicsPageCurrentTopicsDescription');
  }

  private computeTopicsPageCurrentTopicsDescriptionEmpty_(): string {
    return this.i18n(
        this.shouldShowV2_ ? 'topicsPageCurrentTopicsDescriptionEmptyPTB' :
                             'topicsPageCurrentTopicsDescriptionEmpty');
  }

  private computeTopicsPageBlockedTopicsHeading_(): string {
    return this.i18n(
        this.shouldShowV2_ ? 'topicsPageBlockedTopicsHeadingNew' :
                             'topicsPageBlockedTopicsHeading');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-topics-subpage':
        SettingsPrivacySandboxTopicsSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxTopicsSubpageElement.is,
    SettingsPrivacySandboxTopicsSubpageElement);
