// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AppServiceInternalsElement} from './app_service_internals.js';

export function getHtml(this: AppServiceInternalsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>App Service Internals</h1>

<button @click="${this.save_}">Save as .txt</button>

<ul>
  <li>
    <a href="#app-list">App List</a>
    <ul>
      ${this.appList_.map(item => html`
        <li><a href="#app-${item.id}">${item.name}</a></li>
      `)}
    </ul>
  </li>
  <li><a href="#preferred-apps">Preferred Apps</a></li>
  <ul>
    ${this.preferredAppList_.map(item => html`
      <li><a href="#preferred-app-${item.id}">${item.name}</a></li>
    `)}
  </ul>
  <li><a href="#promise-apps">Promise Apps</a></li>
  <li><a href="#app-capabilities">App Capabilities</a></li>
</ul>

<section>
  <h2 id="app-list">App List</h2>

  ${this.appList_.map(item => html`
    <div id="app-${item.id}">
      <h3>${item.name}</h3>
      <img src="chrome://app-icon/${item.id}/128">
      <pre>${item.debugInfo}</pre>
    </div>
  `)}

  <h2 id="preferred-apps">Preferred Apps</h2>

  ${this.preferredAppList_.map(item => html`
    <div id="preferred-app-${item.id}">
      <h3>${item.name}</h3>
      <pre>${item.preferredFilters}</pre>
    </div>
  `)}

  <h2 id="promise-apps">Promise Apps</h2>

  ${this.promiseAppList_.map(item => html`
    <div id="promise-app-${item.packageId}">
      <h3>${item.packageId}</h3>
      <pre>${item.debugInfo}</pre>
    </div>
  `)}

  <h2 id="app-capabilities">App Capabilities</h2>

  ${this.appCapabilityList_.map(item => html`
    <h3>${item.name}</h3>
    <pre>${item.debugInfo}</pre>
  `)}
</section>
<!--_html_template_end_-->`;
  // clang-format on
}
