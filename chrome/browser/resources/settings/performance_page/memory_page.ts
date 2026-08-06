// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import '../settings_page/settings_section.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './memory_page.css.js';
import {getHtml} from './memory_page.html.js';
import {PerformanceBrowserProxyImpl, PerformanceFeedbackCategory} from './performance_browser_proxy.js';
import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {MemorySaverModeState, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';

export const MEMORY_SAVER_MODE_PREF =
    'performance_tuning.high_efficiency_mode.state';

export const MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF =
    'performance_tuning.high_efficiency_mode.aggressiveness';

const SettingsMemoryPageElementBase = PrefServiceObserverMixinLit(CrLitElement);

export interface SettingsMemoryPageElement {
  $: {
    toggleButton: SettingsToggleButtonElement,
  };
}

export class SettingsMemoryPageElement extends SettingsMemoryPageElementBase {
  static get is() {
    return 'settings-memory-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      numericUncheckedValues_: {type: Array},
      numericCheckedValue_: {type: Number},
      memorySaverStatePref_: {type: Object},
    };
  }

  protected accessor numericUncheckedValues_: MemorySaverModeState[] =
      [MemorySaverModeState.DISABLED];
  protected accessor numericCheckedValue_: MemorySaverModeState =
      MemorySaverModeState.ENABLED;
  protected accessor memorySaverStatePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(MEMORY_SAVER_MODE_PREF, 'memorySaverStatePref_');
  }

  protected onMemorySaverModeChange_() {
    this.metricsProxy_.recordMemorySaverModeChanged(
        PrefService.getInstance()
            .getPref<number>(MEMORY_SAVER_MODE_PREF)
            .value);
  }

  protected onMemorySaverModeAggressivenessChange_() {
    this.metricsProxy_.recordMemorySaverModeAggressivenessChanged(
        PrefService.getInstance()
            .getPref<number>(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF)
            .value);
  }

  protected isMemorySaverModeEnabled_(): boolean {
    if (!this.memorySaverStatePref_) {
      return false;
    }
    return this.memorySaverStatePref_.value !== MemorySaverModeState.DISABLED;
  }

  protected onMemorySaverSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('memorySaverLearnMoreUrl'));
  }

  protected showSendFeedbackButton_(): boolean {
    // <if expr="_google_chrome">
    return true;
    // </if>
    // <if expr="not _google_chrome">
    return false;
    // </if>
  }

  protected onSendFeedback_(_e: Event) {
    // <if expr="_google_chrome">
    _e.stopPropagation();
    PerformanceBrowserProxyImpl.getInstance().openFeedbackDialog(
        PerformanceFeedbackCategory.TABS);
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-memory-page': SettingsMemoryPageElement;
  }
}

customElements.define(SettingsMemoryPageElement.is, SettingsMemoryPageElement);
