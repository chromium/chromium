// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './permission_chip.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionChipDelegate} from './permission_chip_delegate.js';
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
      delegate: {type: Object},
      dashboardState: {type: Object},

      // Whether bubbles can be tab-navigated into. This defaults to false
      // since that ensures we will at least make the chip navigable.
      permissionBubblesKeyboardNavigable: {type: Boolean},
    };
  }

  accessor delegate: PermissionChipDelegate|null = null;
  accessor dashboardState: PermissionDashboardState|null = null;
  accessor permissionBubblesKeyboardNavigable: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'permission-dashboard': PermissionDashboardElement;
  }
}

customElements.define(
    PermissionDashboardElement.is, PermissionDashboardElement);
