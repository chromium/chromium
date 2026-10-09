// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ToolbarEvent} from '../content/read_anything_types.js';
import type {AudioBrowserProxy} from '../read_aloud/audio_browser_proxy.js';
import {AudioBrowserProxyImpl} from '../read_aloud/audio_browser_proxy.js';

import {getHtml} from './highlight_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface HighlightMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const HighlightMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the highlight menu.
export class HighlightMenuElement extends HighlightMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'highlight-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      highlightGranularity: {type: Number},
      nonModal: {type: Boolean},
      options_: {type: Array},
    };
  }

  accessor highlightGranularity: number = 0;
  accessor nonModal: boolean = false;

  private audioBrowserProxy_: AudioBrowserProxy =
      AudioBrowserProxyImpl.getInstance();

  protected accessor options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('autoHighlightTitle'),
      data: this.audioBrowserProxy_.getAutoHighlighting(),
    },
    {
      title: loadTimeData.getString('wordHighlightTitle'),
      data: this.audioBrowserProxy_.getWordHighlighting(),
    },
    ...(this.audioBrowserProxy_.isPhraseHighlightingEnabled()?[{
      title: loadTimeData.getString('phraseHighlightTitle'),
      data: this.audioBrowserProxy_.getPhraseHighlighting(),
    }]: []),
    {
      title: loadTimeData.getString('sentenceHighlightTitle'),
      data: this.audioBrowserProxy_.getSentenceHighlighting(),
    },
    {
      title: loadTimeData.getString('noHighlightTitle'),
      data: this.audioBrowserProxy_.getNoHighlighting(),
    },
  ];

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected restoredHighlightIndex_(): number {
    return getIndexOfSetting(this.options_, this.highlightGranularity);
  }

  protected onHighlightChange_() {
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'highlight-menu': HighlightMenuElement;
  }
}

customElements.define(HighlightMenuElement.is, HighlightMenuElement);
