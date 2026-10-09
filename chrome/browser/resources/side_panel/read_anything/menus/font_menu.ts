
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './simple_action_menu.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {VisualBrowserProxy} from '../app/visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';
import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ToolbarEvent} from '../content/read_anything_types.js';

import {getHtml} from './font_menu.html.js';
import {getIndexOrDefault} from './menu_util.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface FontMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const FontMenuElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

// Stores and propagates the data for the font menu used when read aloud is
// enabled.
export class FontMenuElement extends FontMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'font-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      font: {type: String},
      pageLanguage: {type: String},
      areFontsLoaded: {type: Boolean},
      options_: {type: Array},
      nonModal: {type: Boolean},
    };
  }

  protected accessor options_: Array<MenuStateItem<string>> = [];

  accessor areFontsLoaded: boolean = false;
  accessor pageLanguage: string = '';
  accessor font: string = '';
  accessor nonModal: boolean = false;

  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('pageLanguage') ||
        changedProperties.has('areFontsLoaded')) {
      this.setFontOptions_(this.visualBrowserProxy_.getSupportedFonts());
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected currentFontIndex_(): number {
    return getIndexOrDefault(this.options_, this.font);
  }

  protected onFontChange_() {
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }

  private setFontOptions_(fontList: string[]) {
    this.options_ = fontList.map(font => ({
                                   title: this.getFontItemLabel_(font),
                                   data: font,
                                   style: `font-family:${font}`,
                                 }));
  }

  private getFontItemLabel_(font: string): string {
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
    'font-menu': FontMenuElement;
  }
}

customElements.define(FontMenuElement.is, FontMenuElement);
