// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutofillMetadataAppElement} from './autofill_metadata_app.js';

export function getHtml(this: AutofillMetadataAppElement) {
  return html`<!--_html_template_start_-->
<div id="header">
  <h1 id="title">$i18n{autofillMetadataPageTitle}</h1>
</div>
<key-value-pair-viewer ?loading="${this.loading_}" .entries="${this.entries_}">
</key-value-pair-viewer>
<!--_html_template_end_-->`;
}
