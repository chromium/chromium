// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './permission_chip.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './permission_dashboard.css.js';
import {getHtml} from './permission_dashboard.html.js';
import type {PermissionDashboardState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class PermissionDashboardElement extends CrLitElement {
  static get is() {
    return 'permission-dashboard';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dashboardState: {type: Object},
    };
  }

  accessor dashboardState: PermissionDashboardState|null = null;
}

declare global {
  interface HTMLElementTagNameMap {
    'permission-dashboard': PermissionDashboardElement;
  }
}

customElements.define(
    PermissionDashboardElement.is, PermissionDashboardElement);
