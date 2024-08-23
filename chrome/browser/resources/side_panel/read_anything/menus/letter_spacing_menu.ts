// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';
import '../read_anything_toolbar.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ToolbarEvent} from '../common.js';
import {ReadAnythingSettingsChange} from '../metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../read_anything_logger.js';

import {getTemplate} from './letter_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenu} from './simple_action_menu.js';

export interface LetterSpacingMenu {
  $: {
    menu: SimpleActionMenu,
  };
}

const LetterSpacingMenuBase = WebUiListenerMixin(PolymerElement);

// Stores and propagates the data for the letter spacing menu.
export class LetterSpacingMenu extends LetterSpacingMenuBase {
  private options_: Array<MenuStateItem<number>> = [
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

  static get is() {
    return 'letter-spacing-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      options_: Array,
      settingsPrefs: Object,
      toolbarEventEnum_: {
        type: Object,
        value: ToolbarEvent,
      },
    };
  }

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  private onLetterSpacingChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLetterSpacingChange(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE);
  }

  private restoredLetterSpacingIndex_(): number {
    return getIndexOfSetting(this.options_, chrome.readingMode.letterSpacing);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'letter-spacing-menu': LetterSpacingMenu;
  }
}

customElements.define(LetterSpacingMenu.is, LetterSpacingMenu);
