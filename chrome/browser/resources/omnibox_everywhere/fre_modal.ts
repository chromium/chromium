// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './fre_modal.css.js';
import {getHtml} from './fre_modal.html.js';

const OmniboxEverywhereFreModalElementBase = I18nMixinLit(CrLitElement);

export class OmniboxEverywhereFreModalElement extends
    OmniboxEverywhereFreModalElementBase {
  static get is() {
    return 'fre-modal';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onCloseClick_() {
    this.fire('close');
  }

  protected onAcceptHotkeyClick_() {
    this.fire('accept-hotkey');
  }

  protected onSettingsClick_(e?: Event) {
    if (e) {
      e.preventDefault();
    }
    this.fire('open-settings');
  }
}

export type FreModalElement = OmniboxEverywhereFreModalElement;

declare global {
  interface HTMLElementTagNameMap {
    'fre-modal': OmniboxEverywhereFreModalElement;
  }
}

customElements.define(
    OmniboxEverywhereFreModalElement.is, OmniboxEverywhereFreModalElement);
