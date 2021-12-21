// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays the Google Photos zero state.
 */

import './styles.js';
import '../../common/styles.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

export class GooglePhotosZeroState extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-zero-state';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** Returns the message to be displayed. */
  private getMessage_(): string {
    return this.i18nAdvanced('googlePhotosZeroStateMessage', {
      substitutions: [
        '<a target="_blank" href="https://photos.google.com">photos.google.com</a>'
      ]
    });
  }
}

customElements.define(GooglePhotosZeroState.is, GooglePhotosZeroState);
