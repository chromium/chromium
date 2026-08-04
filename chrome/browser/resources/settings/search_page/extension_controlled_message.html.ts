// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ExtensionControlledMessageElement} from './extension_controlled_message.js';

export function getHtml(this: ExtensionControlledMessageElement) {
  return html`<!--_html_template_start_-->
<cr-icon icon="cr:info"></cr-icon>
<div class="flex">
  $i18n{controlledByExtensionTitle}
  <div class="secondary" .innerHTML="${this.getDisclaimerHtml_()}"
      @click="${this.onDisclaimerClick_}">
  </div>
</div>
<!--_html_template_end_-->`;
}
