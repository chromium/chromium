// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './selectable_icon_button.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes} from '../constants.js';
import {TextAlignment} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import {InkTextObserverMixin} from './ink_text_observer_mixin.js';
import {getCss} from './text_alignment_selector.css.js';
import {getHtml} from './text_alignment_selector.html.js';

const TextAlignmentSelectorElementBase = InkTextObserverMixin(CrLitElement);

export class TextAlignmentSelectorElement extends
    TextAlignmentSelectorElementBase {
  static get is() {
    return 'text-alignment-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentAlignment_: {type: String},
    };
  }

  protected accessor currentAlignment_: TextAlignment = TextAlignment.LEFT;

  override focus() {
    const selectedButton = this.shadowRoot.querySelector<HTMLElement>(
        'selectable-icon-button[checked]');
    assert(selectedButton);
    selectedButton.focus();
  }

  protected onSelectedAlignmentChanged_(e: CustomEvent<{value: string}>) {
    const newAlignment = e.detail.value as TextAlignment;
    Ink2Manager.getInstance().setTextAlignment(newAlignment);
  }

  override onTextAttributesChanged(attributes: TextAttributes) {
    this.currentAlignment_ = attributes.alignment;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-alignment-selector': TextAlignmentSelectorElement;
  }
}

customElements.define(
    TextAlignmentSelectorElement.is, TextAlignmentSelectorElement);
