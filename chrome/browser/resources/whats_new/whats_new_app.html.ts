// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WhatsNewAppElement} from './whats_new_app.js';

export function getHtml(this: WhatsNewAppElement) {
  return this.url_ ? html`<iframe id="content" src="${this.url_}"></iframe>` :
                     '';
}
