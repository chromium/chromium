// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes, TextStyles} from '../constants.js';
import {TextStyle} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import {InkTextObserverMixin} from './ink_text_observer_mixin.js';
import {getCss} from './text_styles_selector.css.js';
import {getHtml} from './text_styles_selector.html.js';

const TextStylesSelectorElementBase =
    InkTextObserverMixin(I18nMixinLit(CrLitElement));

export class TextStylesSelectorElement extends TextStylesSelectorElementBase {
  static get is() {
    return 'text-styles-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentStyles_: {type: Object},
    };
  }

  protected accessor currentStyles_: TextStyles = {
    [TextStyle.BOLD]: false,
    [TextStyle.ITALIC]: false,
    [TextStyle.STRIKETHROUGH]: false,
  };

  protected getTextStyles_(): TextStyle[] {
    // `loadTimeData` is guaranteed to be populated here because this element is
    // only rendered after entering text annotation mode, which is gated by
    // `pdfTextAnnotationsEnabled`. Revisit or remove this `loadTimeData` usage
    // if/when text annotation mode is no longer behind a feature flag.
    if (loadTimeData.getBoolean('pdfTextAnnotationsExtraStylesEnabled')) {
      return Object.values(TextStyle);
    }
    return [TextStyle.BOLD, TextStyle.ITALIC];
  }

  protected onStyleButtonClick_(e: Event) {
    const style = (e.target as HTMLElement).dataset['style'] as TextStyle;
    Ink2Manager.getInstance().toggleTextStyle(style);
  }

  protected getActiveClass_(style: TextStyle) {
    return this.currentStyles_[style] ? 'active' : '';
  }

  protected getAriaPressed_(style: TextStyle) {
    return this.currentStyles_[style] ? 'true' : 'false';
  }

  protected getIcon_(style: TextStyle): string {
    switch (style) {
      case TextStyle.BOLD:
        return 'pdf-ink:format-bold';
      case TextStyle.ITALIC:
        return 'pdf-ink:format-italic';
      case TextStyle.STRIKETHROUGH:
        return 'pdf-ink:strikethrough-s';
      default:
        assertNotReachedCase(style);
    }
  }

  protected getTitle_(style: TextStyle) {
    switch (style) {
      case TextStyle.BOLD:
        return this.i18n('ink2TextStyleBold');
      case TextStyle.ITALIC:
        return this.i18n('ink2TextStyleItalic');
      case TextStyle.STRIKETHROUGH:
        return this.i18n('ink2TextStyleStrikethrough');
      default:
        assertNotReachedCase(style);
    }
  }

  override onTextAttributesChanged(attributes: TextAttributes) {
    this.currentStyles_ = attributes.styles;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-styles-selector': TextStylesSelectorElement;
  }
}

customElements.define(TextStylesSelectorElement.is, TextStylesSelectorElement);
