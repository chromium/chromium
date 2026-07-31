// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevInstallDevProxyTabElement} from './install_dev_proxy_tab.js';

export function getHtml(this: IwaDevInstallDevProxyTabElement) {
  // clang-format off
  return html`
<cr-input
    type="url"
    label="Dev Mode Proxy URL"
    placeholder="http://localhost:2137"
    .value="${this.url_}"
    @value-changed="${this.onUrlValueChanged_}"
    @keydown="${this.onInputKeydown_}"
    ?disabled="${this.disabled}"
    ?invalid="${!!this.urlError_}"
    .errorMessage="${this.urlError_}"
    autofocus>
</cr-input>
`;
}
