// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from '//resources/cr_components/composebox/composebox_mixin.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxAimAppElement} from './aim_app.js';

export function getHtml(this: OmniboxAimAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content">
  <cr-omnibox-composebox searchbox-next-enabled id="composebox"
      searchbox-layout-mode="${this.getSearchboxLayoutMode_()}"
      ?disable-caret-color-animation="${!this.caretAnimationsEnabled_}"
      .showMenuOnClick="${false}"
      .shouldShowGhostFiles="${true}"
      .usePecApi="${this.usePecApi_}"
      .contextManagementInComposeboxEnabled="${this.contextManagementInComposeboxEnabled_}"
      .smartComposeEnabled="${this.smartComposeEnabled_}"
      .submitButtonIconType="${SubmitButtonIconType.FORWARD}"
      .isOblongShape="${this.isOblongShape_}"
      .webuiOmniboxSimplificationEnabled="${this.webuiOmniboxSimplificationEnabled_}"
      .showVoiceSearch="${true}"
      .disableVoiceSearchAnimation="${this.disableVoiceSearchAnimation_}"
      @voice-permission-prompt-changed=
          "${this.onVoicePermissionPromptChanged}"
      @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
      @close-composebox="${this.onCloseComposebox_}"
      @composebox-submit="${this.onComposeboxSubmit_}">
  </cr-omnibox-composebox>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
