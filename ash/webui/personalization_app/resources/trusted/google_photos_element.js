// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './styles.js';
import '../common/styles.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {WithPersonalizationStore} from './personalization_store.js';

/** @polymer */
export class GooglePhotos extends WithPersonalizationStore {
  static get is() {
    return 'google-photos';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  constructor() {
    super();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
  }
}

customElements.define(GooglePhotos.is, GooglePhotos);
