// Copyright 2025 The Chromium Authors
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

import {getHtml} from './line_focus_menu.html.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LineFocusMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LineFocusMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the color theme menu.
export class LineFocusMenuElement extends LineFocusMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'line-focus-menu';
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
      title: loadTimeData.getString('lineFocusOffTitle'),
      data: chrome.readingMode.lineFocusOff,
    },
    {
      title: loadTimeData.getString('lineFocusOneLineTitle'),
      data: chrome.readingMode.lineFocusSmallCursorWindow,
      header: loadTimeData.getString('lineFocusWindowHeading'),
    },
    {
      title: loadTimeData.getString('lineFocusThreeLineTitle'),
      data: chrome.readingMode.lineFocusMediumCursorWindow,
    },
    {
      title: loadTimeData.getString('lineFocusFiveLineTitle'),
      data: chrome.readingMode.lineFocusLargeCursorWindow,
    },
    {
      title: loadTimeData.getString('lineFocusStaticLineTitle'),
      data: chrome.readingMode.lineFocusStaticLine,
      header: loadTimeData.getString('lineFocusLineHeading'),
    },
    {
      title: loadTimeData.getString('lineFocusCursorLineTitle'),
      data: chrome.readingMode.lineFocusCursorLine,
    },
  ];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected restoredLineFocusIndex_(): number {
    return getIndexOfSetting(this.options_, this.settingsPrefs['lineFocus']);
  }

  protected onLineFocusChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLineFocusChanged(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_CHANGE);
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-focus-menu': LineFocusMenuElement;
  }
}

customElements.define(LineFocusMenuElement.is, LineFocusMenuElement);
