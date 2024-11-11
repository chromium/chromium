// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-logging-info-bullet' is a bullet point that informs about
 * logging practices. It shows different info depending on the managed state of
 * an AI feature. |pref| must be set to the preference that is bound to the
 * enterprise policy of this AI feature.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import {PrefControlMixin} from '/shared/settings/controls/pref_control_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './ai_logging_info_bullet.html.js';
import {ModelExecutionEnterprisePolicyValue} from './constants.js';

const SettingsAiLoggingInfoBulletBase = PrefControlMixin(PolymerElement);

export class SettingsAiLoggingInfoBullet extends
    SettingsAiLoggingInfoBulletBase {
  static get is() {
    return 'settings-ai-logging-info-bullet';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label_: {
        type: String,
        computed: 'computeLabel_(pref.value)',
      },
    };
  }

  private label_: string;

  private isLoggingDisabledByPolicy_(): boolean {
    return this.pref?.value ===
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING;
  }

  private computeLabel_(): string {
    return this.isLoggingDisabledByPolicy_() ?
        loadTimeData.getString('aiSubpageSublabelLoggingManagedDisabled') :
        loadTimeData.getString('aiSubpageSublabelReviewers');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-logging-info-bullet': SettingsAiLoggingInfoBullet;
  }
}

customElements.define(
    SettingsAiLoggingInfoBullet.is, SettingsAiLoggingInfoBullet);
