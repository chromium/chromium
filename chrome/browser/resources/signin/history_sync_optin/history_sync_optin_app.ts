// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './history_sync_optin_app.html.js';

export class HistorySyncOptinAppElement extends CrLitElement {
  static get is() {
    return 'history-sync-optin-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-optin-app': HistorySyncOptinAppElement;
  }
}

customElements.define(
    HistorySyncOptinAppElement.is, HistorySyncOptinAppElement);
