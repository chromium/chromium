// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'safety-check-extensions' is the settings page module showing
 * extensions that should be reviewed by the user. It will replace
 * the settings-safety-check-extensions-child after launch.
 */

import './safety_check_child.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import type {SettingsSafetyCheckChildElement} from './safety_check_child.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';
import {getTemplate} from './safety_check_extensions.html.js';
import {SafetyCheckExtensionsBrowserProxyImpl} from './safety_check_extensions_browser_proxy.js';

export interface SafetyCheckExtensionsElement {
  $: {
    safetyCheckChild: SettingsSafetyCheckChildElement,
  };
}

const SafetyCheckExtensionsElementBase = WebUiListenerMixin(PolymerElement);

export class SafetyCheckExtensionsElement extends
    SafetyCheckExtensionsElementBase {
  static get is() {
    return 'safety-check-extensions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      displayString_: String,

      safetyCheckIconEnum_: {
        type: Object,
        value: SafetyCheckIconStatus,
      },
    };
  }

  private displayString_: string;

  override connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUiListener(
        'safety-check-extensions-status-changed',
        this.onSafetyCheckExtensionsChanged_.bind(this));
    this.onSafetyCheckExtensionsChanged_();
  }

  private async onSafetyCheckExtensionsChanged_() {
    const numExtensions =
        await SafetyCheckExtensionsBrowserProxyImpl.getInstance()
            .getNumberOfExtensionsThatNeedReview();
    this.displayString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckExtensionsReviewLabel', numExtensions);
  }

  private onButtonClick_() {
    MetricsBrowserProxyImpl.getInstance().recordAction(
        'Settings.SafetyCheck.ReviewExtensionsThroughSafetyCheck');
    OpenWindowProxyImpl.getInstance().openUrl('chrome://extensions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'safety-check-extensions': SafetyCheckExtensionsElement;
  }
}

customElements.define(
    SafetyCheckExtensionsElement.is, SafetyCheckExtensionsElement);
