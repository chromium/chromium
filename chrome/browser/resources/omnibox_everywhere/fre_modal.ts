// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
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

  protected isMac_(): boolean {
    return isMac;
  }

  protected isFuseboxEligible_(): boolean {
    return loadTimeData.getBoolean('isFuseboxEligible');
  }

  protected onCloseClick_() {
    this.fire('close');
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
