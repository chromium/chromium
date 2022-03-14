// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';
import './safety_check_extensions_child.js';
import './safety_check_passwords_child.js';
import './safety_check_safe_browsing_child.js';
import './safety_check_updates_child.js';
// <if expr="_google_chrome and is_win">
import './safety_check_chrome_cleaner_child.js';

// </if>

import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckParentStatus} from './safety_check_browser_proxy.js';
import {getTemplate} from './safety_check_page.html.js';

type ParentChangedEvent = {
  newState: SafetyCheckParentStatus,
  displayString: string,
};

const SettingsSafetyCheckPageElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class SettingsSafetyCheckPageElement extends
    SettingsSafetyCheckPageElementBase {
  static get is() {
    return 'settings-safety-check-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Current state of the safety check parent element. */
      parentStatus_: {
        type: Number,
        value: SafetyCheckParentStatus.BEFORE,
      },

      /** UI string to display for the parent status. */
      parentDisplayString_: String,
    };
  }

  private parentStatus_: SafetyCheckParentStatus;
  private parentDisplayString_: string;
  private safetyCheckBrowserProxy_: SafetyCheckBrowserProxy =
      SafetyCheckBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  /** Timer ID for periodic update. */
  private updateTimerId_: number = -1;

  override connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.PARENT_CHANGED,
        this.onSafetyCheckParentChanged_.bind(this));

    // Configure default UI.
    this.parentDisplayString_ =
        this.i18n('safetyCheckParentPrimaryLabelBefore');

    if (Router.getInstance().getCurrentRoute() === routes.SAFETY_CHECK &&
        Router.getInstance().getQueryParameters().has('activateSafetyCheck')) {
      this.runSafetyCheck_();
    }
  }

  /** Triggers the safety check. */
  private runSafetyCheck_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.RUN_SAFETY_CHECK);
    this.metricsBrowserProxy_.recordAction('Settings.SafetyCheck.Start');

    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
    // Readout new safety check status via accessibility.
    this.fireIronAnnounce_(this.i18n('safetyCheckAriaLiveRunning'));
  }

  private onSafetyCheckParentChanged_(event: ParentChangedEvent) {
    this.parentStatus_ = event.newState;
    this.parentDisplayString_ = event.displayString;
    if (this.parentStatus_ === SafetyCheckParentStatus.CHECKING) {
      // Ensure the re-run button is visible and focus it.
      flush();
      this.focusIconButton_();
    } else if (this.parentStatus_ === SafetyCheckParentStatus.AFTER) {
      // Start periodic safety check parent ran string updates.
      const update = async () => {
        this.parentDisplayString_ =
            await this.safetyCheckBrowserProxy_.getParentRanDisplayString();
      };
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = window.setInterval(update, 60000);
      // Run initial safety check parent ran string update now.
      update();
      // Readout new safety check status via accessibility.
      this.fireIronAnnounce_(this.i18n('safetyCheckAriaLiveAfter'));
    }
  }

  private fireIronAnnounce_(text: string) {
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  private shouldShowParentButton_(): boolean {
    return this.parentStatus_ === SafetyCheckParentStatus.BEFORE;
  }

  private shouldShowParentIconButton_(): boolean {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  }

  private onRunSafetyCheckClick_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.RAN_SAFETY_CHECK);

    this.runSafetyCheck_();
  }

  private focusIconButton_() {
    this.shadowRoot!.querySelector('cr-icon-button')!.focus();
  }

  private shouldShowChildren_(): boolean {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-page': SettingsSafetyCheckPageElement;
  }
}

customElements.define(
    SettingsSafetyCheckPageElement.is, SettingsSafetyCheckPageElement);
