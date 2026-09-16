// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabSearchItemElement} from './tab_search_item.js';

export function getHtml(this: TabSearchItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="iconContainer">
  <div class="favicon" .style="background-image:${this.faviconUrl_()}"></div>
</div>
<div class="text-container" aria-hidden="true">
  <div id="primaryContainer">
    <div id="primaryText" title="${this.data.tab.title}"></div>
    <img id="mediaAlert" display="${this.mediaAlertVisibility_()}"
        class="${this.getMediaAlertImageClass_()}">
  </div>
  <div id="secondaryContainer">
    <!-- We do not leverage a dom-if element as the element highlighting logic
        may trigger before the stamping has taken place -->
    <tab-group-dot id="groupDot" ?hidden="${!this.data.tabGroup}"
        .color="${this.data.tabGroup?.color ?? 0}">
    </tab-group-dot>
    ${this.data.tabGroup?.title ? html`
      <div id="groupTitle"></div>
      <div class="separator">•</div>
    ` : ''}
    <div id="secondaryText" ?hidden="${this.hideUrl}">
      <bdi id="secondaryTextInner"></bdi>
    </div>
    ${!this.hideTimestamp ? html`
      <div class="separator" ?hidden="${!this.data.hostname || this.hideUrl}">•
      </div>
      <div id="secondaryTimestamp">${this.data.tab.lastActiveElapsedText}</div>
    ` : ''}
  </div>
</div>
${this.isCloseable_() ? html`
  <div class="${this.getButtonContainerStyles_()}">
    <cr-icon-button id="closeButton" role="${this.getCloseButtonRole_()}"
        aria-label="${this.ariaLabelForButton_()}"
        iron-icon="${this.closeButtonIcon}" ?noink="${!this.buttonRipples_}"
        no-ripple-on-focus @click="${this.onCloseButtonClick_}"
        title="${this.tooltipForButton_()}" @focus="${this.onCloseButtonFocus_}"
        @blur="${this.onCloseButtonBlur_}">
    </cr-icon-button>
    <cr-tooltip for="closeButton" position="top" offset="0"
        fit-to-visible-bounds manual-mode>
      ${this.tooltipForButton_()}
    </cr-tooltip>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
