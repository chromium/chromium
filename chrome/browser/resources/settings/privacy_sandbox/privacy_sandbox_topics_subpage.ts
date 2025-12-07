// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import './privacy_sandbox_interest_item.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import type {CanonicalTopic, PrivacySandboxBrowserProxy, PrivacySandboxInterest, TopicsState} from './privacy_sandbox_browser_proxy.js';
import {PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_topics_subpage.html.js';
export interface SettingsPrivacySandboxTopicsSubpageElement {
  $: {
    topicsToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxTopicsSubpageElementBase = SettingsViewMixin(
    RouteObserverMixin(I18nMixin(PrefsMixin(PolymerElement))));

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

      blockedTopicsExpanded_: {
        type: Boolean,
        value: false,
        observer: 'onBlockedTopicsExpanded_',
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

      emptyState_: {
        type: Boolean,
        computed: 'computeEmptyState_(' +
            'prefs.privacy_sandbox.m1.topics_enabled.value)',
      },

      /**
       * If true, the Ad Topics Content parity should be shown.
       */
      shouldShowAdTopicsContentParity_: {
        type: Boolean,
        value: false,
      },

      adTopicsToggleSubLabel_: {
        type: String,
        computed:
            'computeAdTopicsToggleSubLabel_(shouldShowAdTopicsContentParity_)',
      },
    };
  }

  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  declare private topicsList_: PrivacySandboxInterest[];
  declare private blockedTopicsList_: PrivacySandboxInterest[];
  private currentChildTopics_: CanonicalTopic[];
  private currentInterest_?: PrivacySandboxInterest;

  declare private isTopicsListLoaded_: boolean;
  declare private emptyState_: boolean;
  declare private blockedTopicsExpanded_: boolean;

  declare private shouldShowBlockTopicDialog_: boolean;
  declare private blockTopicDialogTitle_: string;
  declare private blockTopicDialogBody_: string;
  declare private shouldShowAdTopicsContentParity_: boolean;
  declare private adTopicsToggleSubLabel_: string;

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));
    this.privacySandboxBrowserProxy_
        .shouldShowPrivacySandboxAdTopicsContentParity()
        .then(
            shouldShow => {
              this.shouldShowAdTopicsContentParity_ = shouldShow;
            },
        );
  }

  // Goal is to not show anything but the toggle and disclaimer when the pref is
  // false.
  private computeEmptyState_(): boolean {
    return !this.getPref('privacy_sandbox.m1.topics_enabled').value;
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (newRoute === routes.PRIVACY_SANDBOX_TOPICS) {
      // Updating the TopicsState because it can be changed by being
      // blocked/unblocked in the Manage Topics Page. Need to keep the data
      // between the two pages up to date.
      this.privacySandboxBrowserProxy_.getTopicsState().then(
          state => this.onTopicsStateChanged_(state));
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
    return this.topicsList_.length === 0;
  }

  private isBlockedTopicsListEmpty_(): boolean {
    return this.blockedTopicsList_.length === 0;
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

  private onBlockTopicDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            'settings-simple-confirmation-dialog');
    assert(dialog);
    assert(this.currentInterest_);
    if (dialog.wasConfirmed()) {
      this.updateTopicsStateForSelectedTopic_(this.currentInterest_);
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
    afterNextRender(this, () => {
      if (!this.shadowRoot!.activeElement) {
        this.shadowRoot!.querySelector<HTMLElement>('#blockedTopicsRow')
            ?.focus();
      }
    });
  }

  // This function is run anytime the interest item changes. Which means that it
  // runs when a user blocks/allows a topic.
  private async onInterestChanged_(e: CustomEvent<PrivacySandboxInterest>) {
    this.currentInterest_ = e.detail;
    assert(!this.currentInterest_.site);

    assert(this.currentInterest_.topic);
    assert(this.currentInterest_.topic.displayString);

    // If topic is being unblocked, show toast and update topic state.
    if (this.currentInterest_.removed) {
      const toast = this.shadowRoot!.querySelector('cr-toast');
      assert(toast);
      toast.show();
      this.updateTopicsStateForSelectedTopic_(this.currentInterest_);
      return;
    }

    this.currentChildTopics_ =
        await this.privacySandboxBrowserProxy_.getChildTopicsCurrentlyAssigned(
            this.currentInterest_.topic);
    // Check if currently selected topic to block has active child topics
    // if it does, show simple confirmation dialog.
    if (this.currentChildTopics_.length !== 0) {
      this.blockTopicDialogTitle_ = loadTimeData.getStringF(
          'manageTopicsDialogTitle', this.currentInterest_.topic.displayString);
      this.blockTopicDialogBody_ = loadTimeData.getStringF(
          'manageTopicsDialogBody', this.currentInterest_.topic.displayString);
      this.shouldShowBlockTopicDialog_ = true;
      return;
    }
    // Currently selected topic doesn't have active child topics.
    // Update topics state.
    this.updateTopicsStateForSelectedTopic_(this.currentInterest_);
    this.blockedTopicsExpanded_ = true;
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

  private onHideToastClick_() {
    const toast = this.shadowRoot!.querySelector('cr-toast');
    assert(toast);
    toast.hide();
  }

  private onPrivacyPolicyLinkClicked_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdTopics.PrivacyPolicyLinkClicked');
  }

  private computeAdTopicsToggleSubLabel_(): string {
    return this.i18n(
        this.shouldShowAdTopicsContentParity_ ? 'adTopicsPageToggleSubLabel' :
                                                'topicsPageToggleSubLabel');
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    return new Map([
      [
        routes.PRIVACY_SANDBOX_MANAGE_TOPICS.path,
        '#privacySandboxManageTopicsLinkRow',
      ],
    ]);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
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
