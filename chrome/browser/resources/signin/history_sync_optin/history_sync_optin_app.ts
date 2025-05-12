// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {getCss} from './history_sync_optin_app.css.js';
import {getHtml} from './history_sync_optin_app.html.js';

const HistorySyncOptinAppElementBase = I18nMixinLit(CrLitElement);

export class HistorySyncOptinAppElement extends HistorySyncOptinAppElementBase {
  static get is() {
    return 'history-sync-optin-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      accountImageSrc_: {type: String},
    };
  }

  protected accessor accountImageSrc_: string =
      loadTimeData.getString('accountPictureUrl');

  // TODO(crbug.com/326912202): Wire the keys.
  protected onCancel_() {}
  protected onAccept_() {}
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-optin-app': HistorySyncOptinAppElement;
  }
}

customElements.define(
    HistorySyncOptinAppElement.is, HistorySyncOptinAppElement);
