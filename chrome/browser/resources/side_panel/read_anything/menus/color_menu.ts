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

import {getHtml} from './color_menu.html.js';
import type {MenuStateItem} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface ColorMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const ColorMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the color theme menu.
export class ColorMenuElement extends ColorMenuElementBase {
  static get is() {
    return 'color-menu';
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

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected restoredThemeIndex_(): number {
    return getIndexOfSetting(this.options_, this.settingsPrefs['theme']);
  }

  protected onThemeChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onThemeChange(event.detail.data);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.THEME_CHANGE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'color-menu': ColorMenuElement;
  }
}

customElements.define(ColorMenuElement.is, ColorMenuElement);
