// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './side_panel_ghost_loader.html.js';

/*
 * Element responsible for rendering the side panel ghost loader.
 */
export class SidePanelGhostLoaderElement extends PolymerElement {
  static get is() {
    return 'side-panel-ghost-loader';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      squares: Array,
      darkMode: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('darkMode'),
        reflectToAttribute: true,
      },
    };
  }

  // Whether the loading results should render in dark mode.
  private darkMode: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'side-panel-ghost-loader': SidePanelGhostLoaderElement;
  }
}

customElements.define(
    SidePanelGhostLoaderElement.is, SidePanelGhostLoaderElement);
