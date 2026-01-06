// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getHtml} from './letter_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LetterSpacingMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LetterSpacingMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the letter spacing menu.
export class LetterSpacingMenuElement extends LetterSpacingMenuElementBase
    implements ToolbarMenu {
  static get is() {
    return 'letter-spacing-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;

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


  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected onLetterSpacingChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLetterSpacingChange(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE);
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
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
