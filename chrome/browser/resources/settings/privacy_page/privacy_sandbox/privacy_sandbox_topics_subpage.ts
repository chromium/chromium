// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl, PrivacySandboxInterest, TopicsState} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_topics_subpage.html.js';

export interface SettingsPrivacySandboxTopicsSubpageElement {
  $: {
    topicsToggle: SettingsToggleButtonElement,
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

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getTopicsState().then(
        state => this.onTopicsStateChanged_(state));
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

  private onLearnMoreClick_() {
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
