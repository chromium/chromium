// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
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
  };

  protected getTextStyles_(): TextStyle[] {
    return Object.values(TextStyle);
  }

  protected onStyleButtonClick_(e: Event) {
    const style = (e.target as HTMLElement).dataset['style'] as TextStyle;
    const newStyles = Object.assign({}, this.currentStyles_);
    newStyles[style] = !newStyles[style];
    Ink2Manager.getInstance().setTextStyles(newStyles);
  }

  protected getActiveClass_(style: TextStyle) {
    return this.currentStyles_[style] ? 'active' : '';
  }

  protected getAriaPressed_(style: TextStyle) {
    return this.currentStyles_[style] ? 'true' : 'false';
  }

  protected getTitle_(style: TextStyle) {
    switch (style) {
      case TextStyle.BOLD:
        return this.i18n('ink2TextStyleBold');
      case TextStyle.ITALIC:
        return this.i18n('ink2TextStyleItalic');
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
