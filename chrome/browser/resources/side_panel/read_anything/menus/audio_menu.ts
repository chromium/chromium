// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grouped_action_menu.js';
import '../read_aloud/accent_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import type {AccentMenuElement} from '../read_aloud/accent_menu.js';
import type {AudioBrowserProxy} from '../read_aloud/audio_browser_proxy.js';
import {AudioBrowserProxyImpl} from '../read_aloud/audio_browser_proxy.js';
import {ReadAloudSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getHtml} from './audio_menu.html.js';
import type {GroupedActionMenuElement} from './grouped_action_menu.js';
import {SettingsItemType} from './menu_util.js';
import type {MenuGroup, MenuStateItem, ToolbarMenu} from './menu_util.js';

export interface AudioMenuElement {
  $: {
    menu: GroupedActionMenuElement,
    accentMenu?: AccentMenuElement,
  };
}

const AudioMenuElementBase = WebUiListenerMixinLit(CrLitElement);

export class AudioMenuElement extends AudioMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'audio-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
      groups_: {type: Array},
      enabledLangs: {type: Array},
      availableVoices: {type: Array},
      localeToDisplayName: {type: Object},
      selectedLang: {type: String},
      showAccentMenuDialog_: {type: Boolean},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor enabledLangs: string[] = [];
  accessor availableVoices: SpeechSynthesisVoice[] = [];
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor selectedLang: string = '';
  protected accessor showAccentMenuDialog_: boolean = false;

  private audioBrowserProxy_: AudioBrowserProxy =
      AudioBrowserProxyImpl.getInstance();

  private highlightOptions_: Array<MenuStateItem<number>> = [
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

  protected accessor groups_: Array<MenuGroup<number|string>> = [
    {
      header: {
        title: loadTimeData.getString('voiceLabel'),
        separator: false,
      },
      items: [{
        title: loadTimeData.getString('accentMenuLabel'),
        icon: 'read-anything:translate',
        itemType: SettingsItemType.ACTION,
        data: 'open-accent-menu',
      }],
      eventName: 'open-accent-menu',
    },
    {
      header: {
        title: loadTimeData.getString('voiceHighlightLabel'),
        separator: true,
      },
      items: this.highlightOptions_,
      eventName: ToolbarEvent.HIGHLIGHT_CHANGE,
    },
  ];

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs')) {
      this.updateOptionsForHighlight_();
      this.groups_ = [...this.groups_];
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected onHighlightChange_(event: CustomEvent<{data: number}>) {
    const data = event.detail.data;
    this.audioBrowserProxy_.onHighlightGranularityChanged(data);
    this.logger_.logSpeechSettingsChange(
        ReadAloudSettingsChange.HIGHLIGHT_CHANGE);
    this.logger_.logHighlightGranularity(data);
    this.settingsPrefs = {
      ...this.settingsPrefs,
      highlightGranularity: data,
    };
  }

  protected onOpenAccentMenu_() {
    this.showAccentMenuDialog_ = true;
  }

  protected onAccentMenuClose_(event: CustomEvent<void>) {
    event.preventDefault();
    event.stopPropagation();
    this.showAccentMenuDialog_ = false;
  }

  private updateOptionsForHighlight_() {
    const currentHighlight = this.settingsPrefs.highlightGranularity;
    this.highlightOptions_.forEach(option => {
      option.selected = option.data === currentHighlight;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'audio-menu': AudioMenuElement;
  }
}

customElements.define(AudioMenuElement.is, AudioMenuElement);
