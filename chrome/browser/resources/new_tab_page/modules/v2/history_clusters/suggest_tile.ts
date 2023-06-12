// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/history_clusters_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchQuery} from '../../../history_cluster_types.mojom-webui.js';
import {I18nMixin} from '../../../i18n_setup.js';

import {getTemplate} from './suggest_tile.html.js';

export class SuggestTileModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-suggest-tile-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The visible related search. */
      relatedSearch: {
        type: Object,
      },

      searchUrl_: {
        type: Object,
        computed: `computeSearchUrl_(query)`,
      },
    };
  }

  relatedSearch: SearchQuery;

  private computeSearchUrl_(query: string) {
    return `https://www.google.com/search?q=${encodeURIComponent(query)}`;
  }
}

customElements.define(SuggestTileModuleElement.is, SuggestTileModuleElement);
