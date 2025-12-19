// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '../read_aloud/voice_selection_menu.js';
import '../menus/simple_action_menu.js';
import '../menus/color_menu.js';
import '../menus/font_menu.js';
import '../menus/font_select.js';
import '../menus/line_focus_menu.js';
import '../menus/line_spacing_menu.js';
import '../menus/letter_spacing_menu.js';
import '../menus/highlight_menu.js';
import '../menus/rate_menu.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './immersive_mode_header.css.js';
import {getHtml} from './immersive_mode_header.html.js';

export interface ImmersiveModeHeaderElement {
  $: {
    close: CrIconButtonElement,
  };
}

const ImmersiveModeHeaderElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ImmersiveModeHeaderElement extends ImmersiveModeHeaderElementBase {
  static get is() {
    return 'immersive-mode-header';
  }
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected isImmersiveEnabled_: boolean = false;

  constructor() {
    super();
    this.isImmersiveEnabled_ = chrome.readingMode.isImmersiveEnabled;
  }

  protected onCloseClick_() {
    chrome.readingMode.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'immersive-mode-header': ImmersiveModeHeaderElement;
  }
}

customElements.define(
    ImmersiveModeHeaderElement.is, ImmersiveModeHeaderElement);
