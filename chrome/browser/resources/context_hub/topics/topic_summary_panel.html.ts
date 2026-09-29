// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicSummaryPanelElement} from './topic_summary_panel.js';

export function getHtml(this: TopicSummaryPanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<p id="longDescription">${this.topic?.longDescription || ''}</p>
<!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
<!-- strings are added. -->
<cr-button id="openRelatedTabs" class="tonal-button"
    ?hidden="${!this.hasOpenableUrls_()}"
    @click="${this.onOpenRelatedTabsClick_}">
  Open related tabs
</cr-button>
<!--_html_template_end_-->`;
  // clang-format on
}
