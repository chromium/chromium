// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DeviceLogAppElement} from './app.js';
import {LogLevel} from './browser_proxy.js';

export function getHtml(this: DeviceLogAppElement) {
  return html`<!--_html_template_start_-->
  <div id="header">
    <span>$i18n{autoRefreshText}</span>
  </div>
  <div id="logOptionsContainer">
    <button id="logRefresh" @click="${this.onLogRefreshClick_}">
      $i18n{logRefreshText}
    </button>
    <button id="logClear" @click="${this.onLogClearClick_}">
      $i18n{logClearText}
    </button>
    <span>$i18n{logLevelLabel}</span>
    <select id="logLevelSelect" @change="${this.onLogLevelSelectChange_}">
      <option value="${LogLevel.ERROR}">$i18n{logLevelErrorText}</option>
      <option value="${LogLevel.USER}">$i18n{logLevelUserText}</option>
      <option value="${LogLevel.EVENT}">$i18n{logLevelEventText}</option>
      <option value="${LogLevel.DEBUG}" selected>
        $i18n{logLevelDebugText}
      </option>
    </select>
    <label>
      <input id="logFileinfo" type="checkbox"
          @change="${this.onLogFileinfoChange_}"
          .checked="${this.logFileInfo_}">
      <span>$i18n{logLevelFileinfoText}</span>
    </label>
    <label>
      <input id="logTimedetail" type="checkbox"
          @change="${this.onLogTimedetailChange_}"
          .checked="${this.logTimeDetail_}">
      <span>$i18n{logLevelTimeDetailText}</span>
    </label>
  </div>

  <div id="logCheckboxContainer">
    <button id="logClearTypes" @click="${this.onLogClearTypesClick_}">
      $i18n{logClearTypesText}
    </button>
    ${this.eventTypes_.map(item => html`
      <label>
        <input id="logType${item.type}" type="checkbox"
              .checked="${item.enabled}" .value="${item.type}"
              @change="${this.onLogTypeChange_}">
        <span>${item.label}</span>
      </label>
    `)}
  </div>
  <div id="typeHint">$i18n{autoSelectTypes}</div>
  <div id="logContainer">
    ${
      this.filteredLogs_.length === 0 ?
          html`<span>$i18n{logNoEntriesText}</span>` :
          ``}
    ${
      this.filteredLogs_.map(
          item => html`
      <p>
        <span class="type-tag log-type-${item.type.toLowerCase()}">
          ${item.type}
        </span>
        <span class="level-tag log-level-${
              item.level.toString().toLowerCase()}">
          ${item.level}
        </span>
        <span>${this.getTextForLogEntry_(item)}</span>
      </p>
    `)}
  </div>
  <!--_html_template_end_-->`;
}
