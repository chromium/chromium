// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CredentialNoteElement} from './credential_note.js';

export function getHtml(this: CredentialNoteElement) {
  return html`<!--_html_template_start_-->
<div id="label" class="cr-form-field-label">$i18n{noteLabel}</div>
<div id="noteField" class="input-field">
  <div id="noteValue" role="textbox" ?limit-note="${!this.showNoteFully_}"
      ?empty="${!this.note}">
    <span>${this.getNoteValue_()}</span>
  </div>
  <a id="showMore" href="/" @click="${this.onShowMoreClick_}" hidden>
    $i18n{showMore}
  </a>
</div>
<!--_html_template_end_-->`;
}
