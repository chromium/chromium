// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '/shared/settings/controls/settings_toggle_button.js';
import './privacy_sandbox_interest_item.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl, PrivacySandboxInterest, TopicsState} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_topics_subpage.html.js';

export interface SettingsPrivacySandboxTopicsSubpageElement {
  $: {
    topicsToggle: SettingsToggleButtonElement,
    footer: HTMLElement,
  };
}

const SettingsPrivacySandboxTopicsSubpageElementBase =
    I18nMixin(PrefsMixin(PolymerElement));

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
    };
  }

  private topicsList_: PrivacySandboxInterest[];
  private blockedTopicsList_: PrivacySandboxInterest[];
  private isTopicsListLoaded_: boolean;
  private isLearnMoreDialogOpen_: boolean;
  private blockedTopicsExpanded_: boolean;
  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));

    this.$.footer.querySelectorAll('a').forEach(
        link => link.title = this.i18n('opensInNewTab'));

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_TOPICS_SUBPAGE);
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

  private onInterestChanged_(e: CustomEvent<PrivacySandboxInterest>) {
    const interest = e.detail;
    assert(!interest.site);
    if (interest.removed) {
      this.blockedTopicsList_.splice(
          this.blockedTopicsList_.indexOf(interest), 1);
    } else {
      this.topicsList_.splice(this.topicsList_.indexOf(interest), 1);
      // Move the blocked topic to the blocked section.
      this.blockedTopicsList_.push({topic: interest.topic, removed: true});
      this.blockedTopicsList_.sort(
          (first, second) =>
              first.topic!.displayString < second.topic!.displayString ? -1 :
                                                                         1);
    }
    // This causes the lists to be fully re-rendered, in order to reflect the
    /// interest changes.
    this.topicsList_ = this.topicsList_.slice();
    this.blockedTopicsList_ = this.blockedTopicsList_.slice();
    // If the interest was previously removed, set it to allowed, and vice
    // versa.
    this.privacySandboxBrowserProxy_.setTopicAllowed(
        interest.topic!, /*allowed=*/ interest.removed);

    this.metricsBrowserProxy_.recordAction(
        interest.removed ? 'Settings.PrivacySandbox.Topics.TopicAdded' :
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

  private onBlockedTopicsExpanded_() {
    if (this.blockedTopicsExpanded_) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Topics.BlockedTopicsOpened');
    }
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
