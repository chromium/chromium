// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSafetyHubExtensionsModuleElement} from './extensions_module.js';

export function getHtml(this: SettingsSafetyHubExtensionsModuleElement) {
  return html`<!--_html_template_start_-->
<settings-safety-hub-module header="${this.headerString_}">
  <div slot="button-container">
    <cr-button id="reviewButton" @click="${this.onReviewButtonClick_}">
      $i18n{safetyHubReview}
      <cr-icon icon="cr:open-in-new" class="icon-blue" slot="suffix-icon">
      </cr-icon>
    </cr-button>
  </div>
</settings-safety-hub-module>
<!--_html_template_end_-->`;
}
