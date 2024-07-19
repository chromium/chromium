// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/images/icons.html.js';
import '../strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

interface DisclosureItem {
  icon: string;
  text: string;
}

const DisclosureAppElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

// TODO (b/353825838): implement accept button click handler.
export class DisclosureAppElement extends DisclosureAppElementBase {
  static get is() {
    return 'product-specifications-disclosure-app';
  }

  static override get styles() {
    return getCss();
  }

  protected items_: DisclosureItem[];

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    this.items_ = [
      {
        icon: 'plant',
        text: this.i18n('disclosureAboutItem'),
      },
      {
        icon: 'google',
        text: this.i18n('disclosureAccountItem'),
      },
      {
        icon: 'frame',
        text: this.i18n('disclosureDataItem'),
      },
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-disclosure-app': DisclosureAppElement;
  }
}

customElements.define(DisclosureAppElement.is, DisclosureAppElement);
