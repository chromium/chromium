// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-logging-info-bullet' is a bullet point that informs about
 * logging practices. It shows different info depending on the managed state of
 * an AI feature. |prefKey| must be set to the preference name that is bound to
 * the enterprise policy of this AI feature.
 */
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PrefKeyObserverMixinLit} from '../controls/pref_key_observer_mixin_lit.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './ai_logging_info_bullet.css.js';
import {getHtml} from './ai_logging_info_bullet.html.js';
import {ModelExecutionEnterprisePolicyValue} from './constants.js';

const SettingsAiLoggingInfoBulletElementBase =
    PrefKeyObserverMixinLit(CrLitElement);

export class SettingsAiLoggingInfoBulletElement extends
    SettingsAiLoggingInfoBulletElementBase {
  static get is() {
    return 'settings-ai-logging-info-bullet';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pref: {type: Object},
      loggingManagedDisabledCustomLabel: {type: String},
    };
  }

  protected accessor pref: chrome.settingsPrivate.PrefObject|undefined =
      undefined;
  accessor loggingManagedDisabledCustomLabel: string|null = null;

  protected isLoggingDisabledByPolicy_(): boolean {
    return this.pref?.value ===
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING ||
        this.pref?.value === ModelExecutionEnterprisePolicyValue.DISABLE;
  }

  protected getLabel_(): string {
    if (!this.isLoggingDisabledByPolicy_()) {
      return loadTimeData.getString('aiSubpageSublabelReviewers');
    }
    if (this.loggingManagedDisabledCustomLabel) {
      return this.loggingManagedDisabledCustomLabel;
    }
    return loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled');
  }
}

export type AiLoggingInfoBulletElement = SettingsAiLoggingInfoBulletElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-logging-info-bullet': SettingsAiLoggingInfoBulletElement;
  }
}

customElements.define(
    SettingsAiLoggingInfoBulletElement.is, SettingsAiLoggingInfoBulletElement);
