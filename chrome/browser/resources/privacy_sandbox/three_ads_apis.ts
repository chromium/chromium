// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BaseDialogMixin} from './base_dialog_mixin.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';
import {getHtml} from './three_ads_apis.html.js';

const ThreeAdsApisBase =
    BaseDialogMixin(CrLitElement, PrivacySandboxNotice.kThreeAdsApisNotice);

export class ThreeAdsApis extends ThreeAdsApisBase {
  static get is() {
    return 'three-ads-apis';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'three-ads-apis': ThreeAdsApis;
  }
}

customElements.define(ThreeAdsApis.is, ThreeAdsApis);
