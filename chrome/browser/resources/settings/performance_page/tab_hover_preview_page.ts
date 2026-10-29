// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getHtml} from './tab_hover_preview_page.html.js';

export const HOVER_CARD_IMAGES_PREF =
    'browser.hovercard.image_previews_enabled';
export const HOVER_CARD_MEMORY_USAGE_PREF =
    'browser.hovercard.memory_usage_enabled';

export interface SettingsTabHoverPreviewPageElement {
  $: {
    hoverCardMemoryUsageToggle: SettingsToggleButtonElement,
  };
}

export class SettingsTabHoverPreviewPageElement extends CrLitElement {
  static get is() {
    return 'settings-tab-hover-preview-page';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showHoverCardImagesOption_: {type: Boolean},
    };
  }

  protected accessor showHoverCardImagesOption_: boolean =
      loadTimeData.getBoolean('showHoverCardImagesOption');

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  protected onHoverCardImagesSettingsBooleanControlChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsProxy_.recordHoverCardImagesEnabledChanged(enabled);
  }

  protected onHoverCardMemoryUsageSettingsBooleanControlChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsProxy_.recordHoverCardMemoryUsageEnabledChanged(enabled);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-tab-hover-preview-page': SettingsTabHoverPreviewPageElement;
  }
}

customElements.define(
    SettingsTabHoverPreviewPageElement.is, SettingsTabHoverPreviewPageElement);
