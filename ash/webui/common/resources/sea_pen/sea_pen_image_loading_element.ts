// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the loading message while
 * a thumbnail is clicked and the high-resolution image is still loading.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sea_pen_image_loading_element.html.js';


const SeaPenImageLoadingElementBase = I18nMixin(PolymerElement);

export class SeaPenImageLoadingElement extends SeaPenImageLoadingElementBase {
  static get is() {
    return 'sea-pen-image-loading';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(SeaPenImageLoadingElement.is, SeaPenImageLoadingElement);
