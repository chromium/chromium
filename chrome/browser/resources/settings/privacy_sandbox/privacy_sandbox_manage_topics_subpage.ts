// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_manage_topics_subpage.html.js';

export interface SettingsPrivacySandboxManageTopicsSubpageElement {
  $: {
    explanationText: HTMLElement,
  };
}
const SettingsPrivacySandboxManageTopicsSubpageElementBase =
    I18nMixin(PrefsMixin(PolymerElement));

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
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

    };
  }

  override ready() {
    super.ready();

    this.$.explanationText.querySelectorAll('a').forEach(
        link =>
            link.setAttribute('aria-description', this.i18n('opensInNewTab')));
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
