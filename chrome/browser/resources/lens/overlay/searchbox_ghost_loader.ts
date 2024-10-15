// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '/lens/shared/searchbox_shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './searchbox_ghost_loader.html.js';

const SearchboxGhostLoaderElementBase = I18nMixin(PolymerElement);

// Displays a loading preview while waiting on autocomplete to return matches.
class SearchboxGhostLoaderElement extends SearchboxGhostLoaderElementBase {
  static get is() {
    return 'cr-searchbox-ghost-loader';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}

customElements.define(
    SearchboxGhostLoaderElement.is, SearchboxGhostLoaderElement);
