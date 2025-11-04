// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrefs} from '../shared/common.js';
import {ReadAloudSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {MenuStateItem} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import {getHtml} from './rate_menu.html.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface RateMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

// 3x and 4x speeds are hidden on non-ChromeOS because natural voices on
// non-ChromeOS do not currently support 3x and 4x speeds.
export const RATE_OPTIONS: number[] = chrome.readingMode.isChromeOsAsh ?
    [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4] :
    [0.5, 0.8, 1, 1.2, 1.5, 2];

const RateMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the speech rate menu.
export class RateMenuElement extends RateMenuElementBase {
  static get is() {
    return 'rate-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {settingsPrefs: {type: Object}};
  }

  accessor settingsPrefs: SettingsPrefs = {
    letterSpacing: 0,
    lineSpacing: 0,
    theme: 0,
    speechRate: 0,
    font: '',
    highlightGranularity: 0,
  };

  protected options_: Array<MenuStateItem<number>> = RATE_OPTIONS.map(rate => {
    return {
      title: loadTimeData.getStringF(
          'voiceSpeedOptionTitle', rate.toLocaleString()),
      data: rate,
    };
  });
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected restoredRateIndex_(): number {
    return getIndexOfSetting(this.options_, this.settingsPrefs['speechRate']);
  }

  protected onRateChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onSpeechRateChange(event.detail.data);
    this.logger_.logSpeechSettingsChange(
        ReadAloudSettingsChange.VOICE_SPEED_CHANGE);
    // Log which rate is chosen by index rather than the rate value itself.
    this.logger_.logVoiceSpeed(this.$.menu.currentSelectedIndex);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'Rate-menu': RateMenuElement;
  }
}

customElements.define(RateMenuElement.is, RateMenuElement);
