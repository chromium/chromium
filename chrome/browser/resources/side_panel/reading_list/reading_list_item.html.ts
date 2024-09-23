// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReadingListItemElement} from './reading_list_item.js';

export function getHtml(this: ReadingListItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-url-list-item id="crUrlListItem"
    title="${this.data.title}"
    description="${this.data.displayUrl}"
    reverse-elide-description
    description-meta="${this.data.displayTimeSinceUpdate}"
    url="${this.data.url.url}">
  <cr-icon-button slot="suffix" id="updateStatusButton" disable-ripple
      aria-label="${this.getUpdateStatusButtonTooltip_(
          '$i18n{tooltipMarkAsUnread}', '$i18n{tooltipMarkAsRead}')}"
      iron-icon="${this.getUpdateStatusButtonIcon_('cr:check-circle',
          'read-later:check-circle-outline')}"
      ?noink="${!this.buttonRipples}" no-ripple-on-focus
      @click="${this.onUpdateStatusClick_}"
      title="${this.getUpdateStatusButtonTooltip_('$i18n{tooltipMarkAsUnread}',
          '$i18n{tooltipMarkAsRead}')}">
  </cr-icon-button>
  <cr-icon-button slot="suffix" id="deleteButton"
      aria-label="$i18n{tooltipDelete}"
      iron-icon="cr:close" ?noink="${!this.buttonRipples}" no-ripple-on-focus
      @click="${this.onItemDeleteClick_}" title="$i18n{tooltipDelete}">
  </cr-icon-button>
</cr-url-list-item>
<!--_html_template_end_-->`;
  // clang-format on
}
