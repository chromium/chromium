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
    <svg id="groupSvg" viewBox="-5 -5 10 10" xmlns="http://www.w3.org/2000/svg"
        display="${this.groupSvgDisplay_()}">
      <circle id= "groupDot" cx="0" cy="0" r="4">
    </svg>
    ${this.hasTabGroupWithTitle_() ? html`
      <div id="groupTitle"></div>
      <div class="separator">•</div>
    ` : ''}
    <div id="secondaryText" ?hidden="${this.hideUrl}"></div>
    ${!this.inSuggestedGroup ? html`
      <div class="separator" ?hidden="${!this.data.hostname || this.hideUrl}">•
      </div>
      <div id="secondaryTimestamp">${this.data.tab.lastActiveElapsedText}</div>
    `: ''}
  </div>
</div>
${this.isCloseable_() ? html`
  <div class="${this.getButtonContainerStyles_()}">
    <cr-icon-button id="closeButton" role="${this.getCloseButtonRole_()}"
        aria-label="${this.ariaLabelForButton_()}"
        iron-icon="${this.closeButtonIcon}" ?noink="${!this.buttonRipples_}"
        no-ripple-on-focus @click="${this.onItemClose_}"
        title="${this.tooltipForButton_()}">
    </cr-icon-button>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
