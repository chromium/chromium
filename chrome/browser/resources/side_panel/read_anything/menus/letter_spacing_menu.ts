// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrefs} from '../common.js';
import {ReadAnythingSettingsChange} from '../metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../read_anything_logger.js';

import {getHtml} from './letter_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LetterSpacingMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LetterSpacingMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the letter spacing menu.
export class LetterSpacingMenuElement extends LetterSpacingMenuElementBase {
  static get is() {
    return 'letter-spacing-menu';
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

  protected options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('letterSpacingStandardTitle'),
      icon: 'read-anything:letter-spacing-standard',
      data: chrome.readingMode.standardLetterSpacing,
    },
    {
      title: loadTimeData.getString('letterSpacingWideTitle'),
      icon: 'read-anything:letter-spacing-wide',
      data: chrome.readingMode.wideLetterSpacing,
    },
    {
      title: loadTimeData.getString('letterSpacingVeryWideTitle'),
      icon: 'read-anything:letter-spacing-very-wide',
      data: chrome.readingMode.veryWideLetterSpacing,
    },
  ];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();


  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected onLetterSpacingChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLetterSpacingChange(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE);
  }

  protected restoredLetterSpacingIndex_(): number {
    return getIndexOfSetting(
        this.options_, this.settingsPrefs['letterSpacing']);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'letter-spacing-menu': LetterSpacingMenuElement;
  }
}

customElements.define(LetterSpacingMenuElement.is, LetterSpacingMenuElement);
