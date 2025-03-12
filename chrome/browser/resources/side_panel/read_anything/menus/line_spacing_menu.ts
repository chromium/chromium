// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ReadAnythingSettingsChange} from '../metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../read_anything_logger.js';

import {getHtml} from './line_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenu} from './simple_action_menu.js';

export interface LineSpacingMenuElement {
  $: {
    menu: SimpleActionMenu,
  };
}

const LineSpacingMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the line spacing menu.
export class LineSpacingMenuElement extends LineSpacingMenuElementBase {
  protected options_: Array<MenuStateItem<number>> = [
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

  override render() {
    return getHtml.bind(this)();
  }

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected restoredLineSpacingIndex_(): number {
    return getIndexOfSetting(this.options_, chrome.readingMode.lineSpacing);
  }

  protected onLineSpacingChange_(event: CustomEvent<{data: number}>) {
    chrome.readingMode.onLineSpacingChange(event.detail.data);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-spacing-menu': LineSpacingMenuElement;
  }
}

customElements.define(LineSpacingMenuElement.is, LineSpacingMenuElement);
