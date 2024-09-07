// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';
import './vc_background_breadcrumb_element.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {isManagedSeaPenEnabled} from 'chrome://resources/ash/common/sea_pen/load_time_booleans.js';
import {SeaPenQueryParams} from 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './vc_background_app.html.js';

const VcBackgroundAppBase = I18nMixin(PolymerElement);

export class VcBackgroundApp extends VcBackgroundAppBase {
  static get is() {
    return 'vc-background-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path_: {
        type: String,
        observer: 'onPathChanged_',
      },

      query_: {
        type: String,
      },

      queryParams_: {
        type: Object,
      },
    };
  }
  private path_: string;
  private query_: string;
  private queryParams_: SeaPenQueryParams;

  private onPathChanged_() {
    if (!isManagedSeaPenEnabled()) {
      console.warn('No permission to access VC Background');
      window.close();
      return;
    }
    // Navigates to the top of the subpage.
    window.scrollTo(0, 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'vc-background-app': VcBackgroundApp;
  }
}

customElements.define(VcBackgroundApp.is, VcBackgroundApp);
