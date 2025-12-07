// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LinkContainerElement} from './link_container.js';

export function getHtml(this: LinkContainerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="link" id="systemDialogLink"
    ?actionable="${!this.systemDialogLinkDisabled_}"
    ?hidden="${!this.shouldShowSystemDialogLink_}"
    @click="${this.onSystemDialogClick_}">
  <div class="label">$i18n{systemDialogOption}</div>
  <cr-icon-button class="icon-external"
      ?hidden="${this.openingSystemDialog_}"
      ?disabled="${this.systemDialogLinkDisabled_}"
      aria-label="$i18n{systemDialogOption}"></cr-icon-button>
  <div id="systemDialogThrobber" ?hidden="${!this.openingSystemDialog_}"
      class="throbber"></div>
</div>
<if expr="is_macosx">
<div class="link" id="openPdfInPreviewLink" ?actionable="${!this.disabled}"
    @click="${this.onOpenInPreviewClick_}">
  <div class="label">$i18n{openPdfInPreviewOption}</div>
  <cr-icon-button class="icon-external"
      ?hidden="${this.openingInPreview_}" ?disabled="${this.disabled}"
      aria-label="$i18n{openPdfInPreviewOption}"></cr-icon-button>
  <div id="openPdfInPreviewThrobber" ?hidden="${!this.openingInPreview_}"
      class="throbber"></div>
</div>
</if>
<!--_html_template_end_-->`;
  // clang-format on
}
