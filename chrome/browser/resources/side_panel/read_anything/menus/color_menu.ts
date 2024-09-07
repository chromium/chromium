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

import {getTemplate} from './color_menu.html.js';
import type {MenuStateItem} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import type {SimpleActionMenu} from './simple_action_menu.js';

export interface ColorMenu {
  $: {
    menu: SimpleActionMenu,
  };
}

const ColorMenuBase = WebUiListenerMixin(PolymerElement);

// Stores and propagates the data for the color theme menu.
export class ColorMenu extends ColorMenuBase {
  private options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('defaultColorTitle'),
      icon: 'read-anything-20:default-theme',
      data: chrome.readingMode.defaultTheme,
    },
    {
      title: loadTimeData.getString('lightColorTitle'),
      icon: 'read-anything-20:light-theme',
      data: chrome.readingMode.lightTheme,
    },
    {
      title: loadTimeData.getString('darkColorTitle'),
      icon: 'read-anything-20:dark-theme',
      data: chrome.readingMode.darkTheme,
    },
    {
      title: loadTimeData.getString('yellowColorTitle'),
      icon: 'read-anything-20:yellow-theme',
      data: chrome.readingMode.yellowTheme,
    },
    {
      title: loadTimeData.getString('blueColorTitle'),
      icon: 'read-anything-20:blue-theme',
      data: chrome.readingMode.blueTheme,
    },
  ];

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  static get is() {
    return 'color-menu';
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

  private restoredThemeIndex_(): number {
    return getIndexOfSetting(this.options_, chrome.readingMode.colorTheme);
  }

  private onThemeChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onThemeChange(event.detail.data);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.THEME_CHANGE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'color-menu': ColorMenu;
  }
}

customElements.define(ColorMenu.is, ColorMenu);
