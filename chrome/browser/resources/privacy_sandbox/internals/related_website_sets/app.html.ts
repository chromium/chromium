// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsAppElement} from './app.js';

export function getHtml(this: RelatedWebsiteSetsAppElement) {
  return html`
<div>Input something</div>
<cr-input id="input" .value="${this.myValue}"
    ?disabled="${this.disabled}"
    @value-changed="${this.onInputValueChanged_}">
</cr-input>`;
}