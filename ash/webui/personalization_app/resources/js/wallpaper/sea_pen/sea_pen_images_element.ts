// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the result set of SeaPen
 * wallpapers.
 */

import {WithPersonalizationStore} from '../../personalization_store.js';

import {getTemplate} from './sea_pen_images_element.html.js';

export class SeaPenImagesElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: String,
    };
  }
  private templateId: string;
}

customElements.define(SeaPenImagesElement.is, SeaPenImagesElement);
