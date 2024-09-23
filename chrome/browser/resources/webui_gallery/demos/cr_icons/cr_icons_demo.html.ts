// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrIconsDemoElement} from './cr_icons_demo.js';

export function getHtml(this: CrIconsDemoElement) {
  return html`
<h1>cr-icons from the common cr-iconsets defined in icons_lit.html</h1>
<div class="demos">
  <div>Commonly used cr-icons across WebUI built with SVG.</div>
  <div class="icons" .style="
      --iron-icon-fill-color: ${this.iconColor_};
      --iron-icon-height: ${this.iconSize_}px;
      --iron-icon-width: ${this.iconSize_}px;
  ">
    ${this.icons_.map(icon => html`
      <div class="icon">
        <cr-icon icon="${icon}"></cr-icon>
        <div class="label">${icon}</div>
      </div>
    `)}
  </div>
</div>

<h1>cr-icons sourced from a custom cr-iconset</h1>
<div class="demos">
  <div>An example of a custom iconset for an app using cr-iconset.</div>
  <cr-iconset name="desserts">
    <svg>
      <defs>
        <g id="cake"><path d="M4 22q-.425 0-.712-.288Q3 21.425 3 21v-5q0-.825.587-1.413Q4.175 14 5 14v-4q0-.825.588-1.413Q6.175 8 7 8h4V6.55q-.45-.3-.725-.725Q10 5.4 10 4.8q0-.375.15-.738.15-.362.45-.662L12 2l1.4 1.4q.3.3.45.662.15.363.15.738 0 .6-.275 1.025-.275.425-.725.725V8h4q.825 0 1.413.587Q19 9.175 19 10v4q.825 0 1.413.587Q21 15.175 21 16v5q0 .425-.288.712Q20.425 22 20 22Zm3-8h10v-4H7Zm-2 6h14v-4H5Zm2-6h10Zm-2 6h14Zm14-6H5h14Z"></g>
      </defs>
    </svg>
  </cr-iconset>
  <div class="icons" .style="
      --iron-icon-fill-color: ${this.iconColor_};
      --iron-icon-height: ${this.iconSize_}px;
      --iron-icon-width: ${this.iconSize_}px;
  ">
    <div class="icon">
      <cr-icon icon="desserts:cake"></cr-icon>
      <div class="label">desserts:cake</div>
    </div>
  </div>
</div>

<h1>CSS classes for icons, defined in cr_icons.css</h1>
<div class="demos">
  <div>CSS classes to display icons, typically for cr-icon-button.</div>
  <div class="icons" .style="
      --cr-icon-color: ${this.iconColor_};
      --cr-icon-ripple-size: ${this.iconSize_}px;
      --cr-icon-size: ${this.iconSize_}px;
  ">
    ${this.crIcons_.map(icon => html`
      <div class="icon">
        <div class="cr-icon no-overlap ${icon}"></div>
        <div class="label">.${icon}</div>
      </div>
    `)}
  </div>
</div>

<h1>Custom controls</h1>
<div class="demos">
  <cr-input type="number" min="12" max="128" .value="${this.iconSize_}"
      @value-changed="${this.onIconSizeChanged_}"
      label="Icon size"></cr-input>

  <label>
    <input type="color" .value="${this.iconColor_}"
        @input="${this.onIconColorInput_}">
    Icon fill color
  </label>
</div>`;
}
