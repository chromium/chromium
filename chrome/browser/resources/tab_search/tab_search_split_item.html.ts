// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {SplitTabLayout} from './tab_search.mojom-webui.js';
import type {TabSearchSplitItemElement} from './tab_search_split_item.js';

export function getHtml(this: TabSearchSplitItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="iconContainer">
  <div class="split-favicons ${this.data.layout === SplitTabLayout.kStacked ?
      'stacked' : 'side-by-side'}">
    ${this.data.tabUrls.slice(0, 2).map((url: string, index: number) => html`
      <div class="split-favicon"
          .style="background-image: ${this.getFaviconUrl_(url, index)}"></div>
    `)}
  </div>
</div>
<div class="text-container" aria-hidden="true">
  <div id="primaryContainer">
    <div id="primaryText" title="${this.data.title}">${this.data.title}</div>
    ${this.data.tabs ? this.data.tabs.map((tab) => html`
      ${this.hasMediaAlertForTab_(tab) ? html`
        <img class="media-alert
            ${this.getMediaAlertImageClassForTab_(tab)}">
      ` : ''}
    `) : ''}
  </div>
  <div id="secondaryTextContainer">
    <svg id="groupSvg" viewBox="-5 -5 10 10" xmlns="http://www.w3.org/2000/svg"
        display="${this.groupSvgDisplay_()}"
        style="--group-dot-color: ${this.getGroupColor_()}">
      <circle id="groupDot" cx="0" cy="0" r="4" />
    </svg>
    ${this.domainTexts_.slice(0, 2).map((domainText, index: number) => html`
      ${index > 0 ? html`<div class="separator">•</div>` : ''}
      <div class="domain-text" title="${domainText}">
        <bdi>${domainText}</bdi>
      </div>
    `)}
    <div class="separator">•</div>
    <div id="timestamp">${this.data.lastActiveElapsedText}</div>
  </div>
</div>
${this.isCloseable_() ? html`
  <div class="${this.getButtonContainerStyles_()}">
    <cr-icon-button id="closeButton" role="${this.getCloseButtonRole_()}"
        aria-label="${this.ariaLabelForButton_()}"
        iron-icon="${this.closeButtonIcon}" ?noink="${!this.buttonRipples_}"
        no-ripple-on-focus @click="${this.onCloseButtonClick_}"
        title="${this.tooltipForButton_()}"
        @focus="${this.onCloseButtonFocus_}"
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
