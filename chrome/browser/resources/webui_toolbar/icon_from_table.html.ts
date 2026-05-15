// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IconFromTableElement} from './icon_from_table.js';
import {IconType} from './toolbar_ui_api_data_model.mojom-webui.js';

export function getHtml(this: IconFromTableElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  ${this.iconInfo_.type === IconType.kIconSet ?
      html`<cr-icon .icon="${this.iconInfo_.urlOrName}"></cr-icon>` :
    this.iconInfo_.type === IconType.kMaskUrl ?
      html`<div id="maskIconContainer"
            style="mask-image: url(${this.iconInfo_.urlOrName});">
         </div>` :
    html`<div id="colorfulIconContainer"
          style="background-image: url(${this.iconInfo_.urlOrName});">
         </div>`}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
