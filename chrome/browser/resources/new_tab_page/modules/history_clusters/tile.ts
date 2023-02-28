// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/history_clusters_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin} from '../../i18n_setup.js';

import {getTemplate} from './tile.html.js';

export class TileModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-tile';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The visit to display. */
      visit: Object,

      // TODO(crbug.com/1419917): Image service integration.
      /* The icon to display. */
      iconStyle_: String,

      /* The label to display. */
      label_: {
        type: String,
        computed: `computeLabel_(visit.urlForDisplay)`,
      },
    };
  }

  visit: URLVisit;
  cluster: Cluster;

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  private computeLabel_(): string {
    return this.visit.urlForDisplay;
  }
}

customElements.define(TileModuleElement.is, TileModuleElement);
