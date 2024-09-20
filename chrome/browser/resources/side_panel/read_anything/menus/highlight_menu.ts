// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';
import '../read_anything_toolbar.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ToolbarEvent} from '../common.js';

import {getTemplate} from './highlight_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenu} from './simple_action_menu.js';

export interface HighlightMenu {
  $: {
    menu: SimpleActionMenu,
  };
}

const HighlightMenuBase = WebUiListenerMixin(PolymerElement);

// Stores and propagates the data for the highlight menu.
export class HighlightMenu extends HighlightMenuBase {
  // TODO(b/362203467): Apply this setting to the app UI.
  private options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('autoHighlightTitle'),
      data: chrome.readingMode.autoHighlighting,
    },
    {
      title: loadTimeData.getString('wordHighlightTitle'),
      data: chrome.readingMode.wordHighlighting,
    },
    {
      title: loadTimeData.getString('phraseHighlightTitle'),
      data: chrome.readingMode.phraseHighlighting,
    },
    {
      title: loadTimeData.getString('sentenceHighlightTitle'),
      data: chrome.readingMode.sentenceHighlighting,
    },
    {
      title: loadTimeData.getString('noHighlightTitle'),
      data: chrome.readingMode.noHighlighting,
    },
  ];

  static get is() {
    return 'highlight-menu';
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

  private restoredHighlightIndex_(): number {
    return getIndexOfSetting(
        this.options_, chrome.readingMode.highlightGranularity);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'highlight-menu': HighlightMenu;
  }
}

customElements.define(HighlightMenu.is, HighlightMenu);
