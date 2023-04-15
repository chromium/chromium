// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './healthd_internals_shared.css.js';

import {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface HealthdInternalsAppElement {
  $: {
    selector: CrMenuSelector,
  };
}

export class HealthdInternalsAppElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pages: {
        type: Array,
        value: function() {
          return [
            {
              name: 'Telemetry',
              path: '/telemetry',
            },
            {
              name: 'Diagnostics',
              path: '/diagnostics',
            },
            {
              name: 'Event',
              path: '/event',
            },
          ];
        },
      },

      path_: {
        type: String,
        observer: 'pathChanged_',
      },

      selectedIndex_: {
        type: Number,
        value: 0,
        observer: 'selectedIndexChanged_',
      },
    };
  }

  pages: Array<{name: string, path: string}>;
  private path_: string;
  private selectedIndex_: number;

  private pathChanged_() {
    this.selectedIndex_ =
        Math.max(0, this.pages.findIndex(pages => pages.path === this.path_));
  }

  private selectedIndexChanged_() {
    this.path_ = this.pages[this.selectedIndex_]!.path;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
