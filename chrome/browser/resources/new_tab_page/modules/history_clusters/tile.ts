// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/history_clusters_shared_style.css.js';
import './page_favicon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {URLVisit} from '../../history_cluster_types.mojom-webui.js';
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

  private computeLabel_(): string {
    let domain = (new URL(this.visit.normalizedUrl.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }
}

customElements.define(TileModuleElement.is, TileModuleElement);
