// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/497887993): Remove this when cleaning up the shared composebox
// component.
// eslint-disable-next-line no-restricted-imports
import {SubmitButtonIconType} from '//resources/cr_components/composebox/composebox.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxAimAppElement} from './aim_app.js';

export function getHtml(this: OmniboxAimAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content">
  ${this.composeboxForkEnabled_ ? html`
  <cr-omnibox-composebox searchbox-next-enabled id="composebox"
      searchbox-layout-mode="${this.getSearchboxLayoutMode_()}">
  </cr-omnibox-composebox>` : html`
  <cr-composebox id="composebox" searchbox-next-enabled
      .submitButtonIconType="${SubmitButtonIconType.FORWARD}"
      searchbox-layout-mode="${this.getSearchboxLayoutMode_()}"
      ?disable-caret-color-animation="${!this.caretAnimationsEnabled_}"
      ?disable-composebox-animation="${this.disableComposeboxAnimation_}"
      .disableVoiceSearchAnimation="${this.disableVoiceSearchAnimation_}"
      @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
      @close-composebox="${this.onCloseComposebox_}"
      @composebox-submit="${this.onComposeboxSubmit_}"
      .showMenuOnClick="${false}"
      .shouldShowGhostFiles="${true}"
      .showVoiceSearch="${true}"
      entrypoint-name="Omnibox">
  </cr-composebox>`}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
