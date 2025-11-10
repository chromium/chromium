
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './simple_action_menu.js';
import '//resources/cr_elements/md_select.css.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../shared/common.js';
import type {SettingsPrefs} from '../shared/common.js';
import {isForwardArrow, isHorizontalArrow} from '../shared/keyboard_util.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getCss} from './font_select.css.js';
import {getHtml} from './font_select.html.js';
import {getIndexOrDefault} from './menu_util.js';
import type {MenuStateItem} from './menu_util.js';

export interface FontSelectElement {
  $: {
    select: HTMLSelectElement,
  };
}

const NEXT_PREVIOUS_KEYS = ['ArrowUp', 'ArrowDown'];
const OPEN_SELECT_KEYS = [' ', 'Enter'];

const FontSelectElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

// Stores and propagates the data for the font select element used when read
// aloud is disabled.
export class FontSelectElement extends FontSelectElementBase {
  static get is() {
    return 'font-select';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      options: {type: Array},
      settingsPrefs: {type: Object},
      pageLanguage: {type: String},
      areFontsLoaded: {type: Boolean},
    };
  }

  accessor options: Array<MenuStateItem<string>> = [];
  accessor areFontsLoaded: boolean = false;
  accessor pageLanguage: string = '';
  accessor settingsPrefs: SettingsPrefs = {
    letterSpacing: 0,
    lineSpacing: 0,
    theme: 0,
    speechRate: 0,
    font: '',
    highlightGranularity: 0,
  };

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('pageLanguage') ||
        changedProperties.has('areFontsLoaded') ||
        changedProperties.has('settingsPrefs')) {
      this.setFontOptions_(chrome.readingMode.supportedFonts);
    } else if (changedProperties.has('options')) {
      this.$.select.selectedIndex =
          getIndexOrDefault(this.options, chrome.readingMode.fontName);
    }
  }

  constructor() {
    super();
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  protected onFontSelectValueChange_(event: Event) {
    const data = (event.target as HTMLSelectElement).value;
    chrome.readingMode.onFontChange(data);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.FONT_CHANGE);
    this.fire(ToolbarEvent.FONT, {data});
  }

  protected onKeyDown_(e: KeyboardEvent) {
    // The default behavior goes to the next select option. However, we want
    // to instead go to the next toolbar button (handled in onToolbarKeyDown_).
    // ArrowDown and ArrowUp will still move to the next/previous option.
    if (isHorizontalArrow(e.key)) {
      e.preventDefault();
    } else if (NEXT_PREVIOUS_KEYS.includes(e.key)) {
      e.preventDefault();
      const direction = isForwardArrow(e.key) ? 1 : -1;
      const length = this.$.select.options.length;
      this.$.select.selectedIndex =
          (this.$.select.selectedIndex + direction + length) % length;
      this.$.select.dispatchEvent(new Event('change', {bubbles: true}));
    } else if (OPEN_SELECT_KEYS.includes(e.key)) {
      e.preventDefault();
      this.$.select.showPicker();
    }
  }

  private setFontOptions_(fontList: string[]) {
    this.options = fontList.map(font => ({
                                  title: this.getFontItemLabel_(font),
                                  data: font,
                                  style: 'font-family:' + font,
                                }));
  }

  protected getFontItemLabel_(font: string): string {
    // Before fonts are loaded, append the loading text to the font names
    // so that the names will appear in the font menu like:
    // Poppins (loading).
    return this.areFontsLoaded ?
        `${font}` :
        `${font}\u00A0${this.i18n('readingModeFontLoadingText')}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'font-select': FontSelectElement;
  }
}

customElements.define(FontSelectElement.is, FontSelectElement);
