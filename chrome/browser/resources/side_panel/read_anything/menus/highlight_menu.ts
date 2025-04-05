// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrefs} from '../common.js';

import {getHtml} from './highlight_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface HighlightMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const HighlightMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the highlight menu.
export class HighlightMenuElement extends HighlightMenuElementBase {
  static get is() {
    return 'highlight-menu';
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

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected restoredHighlightIndex_(): number {
    return getIndexOfSetting(
        this.options_, this.settingsPrefs['highlightGranularity']);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'highlight-menu': HighlightMenuElement;
  }
}

customElements.define(HighlightMenuElement.is, HighlightMenuElement);
