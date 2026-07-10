// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './privacy_sandbox_icons.html.js';
import '../settings_page/settings_subpage.js';
import '../simple_confirmation_dialog.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {FirstLevelTopicsState, PrivacySandboxBrowserProxy, PrivacySandboxInterest} from './privacy_sandbox_browser_proxy.js';
import {PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_manage_topics_subpage.html.js';

export interface SettingsPrivacySandboxManageTopicsSubpageElement {
  $: {
    explanationText: HTMLElement,
  };
}
const SettingsPrivacySandboxManageTopicsSubpageElementBase = SettingsViewMixin(
    RouteObserverMixin(I18nMixin(PrefsMixin(PolymerElement))));

// First Level Topics for Taxonomy v2
// This list comes from here:
// https://github.com/patcg-individual-drafts/topics/blob/main/taxonomy_v2.md
const topicIdToIconName: Map<number, {newIcon: string, oldIcon: string}> =
    new Map<number, {newIcon: string, oldIcon: string}>([
      [
        1,
        {
          newIcon: 'firstLevelTopics20:artist',
          oldIcon: 'firstLevelTopics20:artist-old',
        },
      ],
      [
        57,
        {
          newIcon: 'firstLevelTopics20:directions-car',
          oldIcon: 'firstLevelTopics20:directions-car-old',
        },
      ],
      [
        86,
        {
          newIcon: 'firstLevelTopics20:health-and-beauty',
          oldIcon: 'firstLevelTopics20:health-and-beauty-old',
        },
      ],
      [
        100,
        {
          newIcon: 'firstLevelTopics20:menu-book',
          oldIcon: 'firstLevelTopics20:menu-book-old',
        },
      ],
      [
        103,
        {
          newIcon: 'firstLevelTopics20:business-center',
          oldIcon: 'firstLevelTopics20:business-center-old',
        },
      ],
      [
        126,
        {
          newIcon: 'firstLevelTopics20:keyboard',
          oldIcon: 'firstLevelTopics20:keyboard-old',
        },
      ],
      [
        149,
        {
          newIcon: 'firstLevelTopics20:finance-mode',
          oldIcon: 'firstLevelTopics20:finance-mode-old',
        },
      ],
      [
        172,
        {
          newIcon: 'firstLevelTopics20:fastfood',
          oldIcon: 'firstLevelTopics20:fastfood-old',
        },
      ],
      [
        180,
        {
          newIcon: 'firstLevelTopics20:videogame-asset',
          oldIcon: 'firstLevelTopics20:videogame-asset-old',
        },
      ],
      [
        196,
        {
          newIcon: 'firstLevelTopics20:sailing',
          oldIcon: 'firstLevelTopics20:sailing-old',
        },
      ],
      [
        207,
        {
          newIcon: 'firstLevelTopics20:home-and-garden',
          oldIcon: 'firstLevelTopics20:home-and-garden-old',
        },
      ],
      [
        215,
        {
          newIcon: 'firstLevelTopics20:bigtop-updates',
          oldIcon: 'firstLevelTopics20:bigtop-updates-old',
        },
      ],
      [
        226,
        {
          newIcon: 'firstLevelTopics20:school',
          oldIcon: 'firstLevelTopics20:school-old',
        },
      ],
      [
        239,
        {
          newIcon: 'firstLevelTopics20:gavel',
          oldIcon: 'firstLevelTopics20:gavel-old',
        },
      ],
      [
        243,
        {
          newIcon: 'firstLevelTopics20:newsmode',
          oldIcon: 'firstLevelTopics20:newsmode-old',
        },
      ],
      [
        250,
        {
          newIcon: 'firstLevelTopics20:communities',
          oldIcon: 'firstLevelTopics20:communities-old',
        },
      ],
      [
        254,
        {
          newIcon: 'firstLevelTopics20:crowdsource',
          oldIcon: 'firstLevelTopics20:crowdsource-old',
        },
      ],
      [
        263,
        {
          newIcon: 'firstLevelTopics20:pets',
          oldIcon: 'firstLevelTopics20:pets-old',
        },
      ],
      [
        272,
        {
          newIcon: 'firstLevelTopics20:real-estate-agent',
          oldIcon: 'firstLevelTopics20:real-estate-agent-old',
        },
      ],
      [
        289,
        {
          newIcon: 'firstLevelTopics20:shopping-bag',
          oldIcon: 'firstLevelTopics20:shopping-bag-old',
        },
      ],
      [
        299,
        {
          newIcon: 'firstLevelTopics20:sports-and-outdoors',
          oldIcon: 'firstLevelTopics20:sports-and-outdoors-old',
        },
      ],
      [
        332,
        {
          newIcon: 'firstLevelTopics20:travel',
          oldIcon: 'firstLevelTopics20:travel-old',
        },
      ],
    ]);

export class SettingsPrivacySandboxManageTopicsSubpageElement extends
    SettingsPrivacySandboxManageTopicsSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-manage-topics-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      firstLevelTopicsList_: {
        type: Array,
        value() {
          return [];
        },
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
    };
  }

  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  declare private firstLevelTopicsList_: PrivacySandboxInterest[];
  private topicBeingToggled_?: PrivacySandboxInterest;
  declare private blockTopicDialogTitle_: string;
  declare private blockTopicDialogBody_: string;
  declare private shouldShowBlockTopicDialog_: boolean;

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getFirstLevelTopics().then(
        state => this.onFirstLevelTopicsStateChanged_(state));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (newRoute === routes.PRIVACY_SANDBOX_MANAGE_TOPICS) {
      // Should not be able to navigate to Manage Topics page when topics is
      // disabled.
      if (!this.getPref('privacy_sandbox.m1.topics_enabled').value) {
        Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
        return;
      }
      // Updating the FirstLevelTopicsState because it can be changed by being
      // blocked/unblocked in the Ad Topics Page. Need to keep the data between
      // the two pages up to date.
      this.privacySandboxBrowserProxy_.getFirstLevelTopics().then(
          state => this.onFirstLevelTopicsStateChanged_(state));
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Topics.Manage.PageOpened');
    }
  }

  private onFirstLevelTopicsStateChanged_(state: FirstLevelTopicsState) {
    const blockedTopicsList = state.blockedTopics.map(topic => {
      return {topic, removed: true};
    });
    this.firstLevelTopicsList_ = state.firstLevelTopics.map(firstLevelTopic => {
      return {
        topic: firstLevelTopic,
        removed: blockedTopicsList.some(
            interest => interest.topic?.topicId === firstLevelTopic.topicId),
      };
    });
  }

  // When the user clicks anywhere on the toggle row, we click the toggle itself
  // here to trigger its on-change event.
  private onToggleRowClick_(e: DomRepeatEvent<PrivacySandboxInterest>) {
    e.stopPropagation();
    assert(e.model.item?.topic);
    const toggleId = `#toggle-${e.model.item.topic.topicId}`;
    const toggleBeingChanged =
        this.shadowRoot!.querySelector<CrToggleElement>(toggleId);
    assert(toggleBeingChanged);
    toggleBeingChanged.click();
  }

  private async onToggleChange_(e: DomRepeatEvent<PrivacySandboxInterest>) {
    e.stopPropagation();
    this.topicBeingToggled_ = e.model.item;
    assert(this.topicBeingToggled_);
    assert(this.topicBeingToggled_.topic);
    const toggleId = `#toggle-${this.topicBeingToggled_.topic.topicId}`;
    const toggleBeingChanged =
        this.shadowRoot!.querySelector<CrToggleElement>(toggleId);
    assert(toggleBeingChanged);
    // At this point, the toggle checked state has already changed. If the
    // toggle is now checked, then the First Level Topic needs to be updated to
    // be unblocked.
    if (toggleBeingChanged.checked) {
      this.updateTopicState_({blocked: false});
      return;
    }
    // Check if the attempted blocked topic has active child topics
    const childTopics =
        await this.privacySandboxBrowserProxy_.getChildTopicsCurrentlyAssigned(
            this.topicBeingToggled_.topic);
    if (childTopics.length !== 0) {
      this.blockTopicDialogTitle_ = loadTimeData.getStringF(
          'manageTopicsDialogTitle',
          this.topicBeingToggled_.topic.displayString);
      this.blockTopicDialogBody_ = loadTimeData.getStringF(
          'manageTopicsDialogBody',
          this.topicBeingToggled_.topic.displayString);
      this.shouldShowBlockTopicDialog_ = true;
      return;
    }
    // Blocking a topic without any assigned children. Should update the new
    // blocked state with the privacySandboxProxy.
    this.updateTopicState_({blocked: true});
  }

  // Changes the state of the first level topic being toggled to be
  // blocked/unblocked. Calls the privacySandboxProxy to set the topic to
  // allowed/disallowed.
  private updateTopicState_(blockedOptions: {blocked: boolean}) {
    assert(this.topicBeingToggled_);
    assert(this.topicBeingToggled_.topic);

    this.topicBeingToggled_.removed = blockedOptions.blocked;
    this.firstLevelTopicsList_ = this.firstLevelTopicsList_.slice();
    this.privacySandboxBrowserProxy_.setTopicAllowed(
        this.topicBeingToggled_.topic, !blockedOptions.blocked);
    this.topicBeingToggled_ = undefined;
    if (blockedOptions.blocked) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Topics.Manage.TopicBlocked');
    } else {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Topics.Manage.TopicEnabled');
    }
  }

  private onBlockTopicDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    if (dialog.wasConfirmed()) {
      this.onBlockButtonDialogHandler_();
    } else {
      this.onCancelButtonDialogHandler_();
    }
    this.blockTopicDialogBody_ = '';
    this.blockTopicDialogTitle_ = '';
    this.topicBeingToggled_ = undefined;
    this.shouldShowBlockTopicDialog_ = false;
  }

  private onCancelButtonDialogHandler_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingCanceled');
    // This causes the list to be fully re-rendered, in order to revert the
    // toggle back to being checked after the user decides to block the topic.
    this.firstLevelTopicsList_ = this.firstLevelTopicsList_.map(topic => {
      return {
        ...topic,
      };
    });
  }

  private onBlockButtonDialogHandler_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Topics.Manage.TopicBlockingConfirmed');
    this.updateTopicState_({blocked: true});
  }

  private onLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Topics.Manage.LearnMoreClicked');
  }

  private computeTopicIcon_(topicId: number): string {
    const isRounded = loadTimeData.getBoolean('webuiRoundedIconsEnabled');
    const entry = topicIdToIconName.get(topicId);
    if (entry) {
      return isRounded ? entry.newIcon : entry.oldIcon;
    }
    return isRounded ? 'firstLevelTopics20:category' :
                       'firstLevelTopics20:category-old';
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-manage-topics-subpage':
        SettingsPrivacySandboxManageTopicsSubpageElement;
  }
}
customElements.define(
    SettingsPrivacySandboxManageTopicsSubpageElement.is,
    SettingsPrivacySandboxManageTopicsSubpageElement);
