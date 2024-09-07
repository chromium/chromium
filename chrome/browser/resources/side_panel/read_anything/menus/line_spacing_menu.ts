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

import {getTemplate} from './line_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenu} from './simple_action_menu.js';

export interface LineSpacingMenu {
  $: {
    menu: SimpleActionMenu,
  };
}

const LineSpacingMenuBase = WebUiListenerMixin(PolymerElement);

// Stores and propagates the data for the line spacing menu.
export class LineSpacingMenu extends LineSpacingMenuBase {
  private options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('lineSpacingStandardTitle'),
      icon: 'read-anything:line-spacing-standard',
      data: chrome.readingMode.standardLineSpacing,
    },
    {
      title: loadTimeData.getString('lineSpacingLooseTitle'),
      icon: 'read-anything:line-spacing-loose',
      data: chrome.readingMode.looseLineSpacing,
    },
    {
      title: loadTimeData.getString('lineSpacingVeryLooseTitle'),
      icon: 'read-anything:line-spacing-very-loose',
      data: chrome.readingMode.veryLooseLineSpacing,
    },
  ];

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  static get is() {
    return 'line-spacing-menu';
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

  private restoredLineSpacingIndex_(): number {
    return getIndexOfSetting(this.options_, chrome.readingMode.lineSpacing);
  }

  private onLineSpacingChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLineSpacingChange(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-spacing-menu': LineSpacingMenu;
  }
}

customElements.define(LineSpacingMenu.is, LineSpacingMenu);
