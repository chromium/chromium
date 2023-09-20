// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays template query to search for
 * SeaPen wallpapers.
 */

import {WithPersonalizationStore} from '../../personalization_store.js';

import {getTemplate} from './sea_pen_template_query_element.html.js';

export class SeaPenTemplateQueryElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-template-query';
  }
  static get template() {
    return getTemplate();
  }
}

customElements.define(
    SeaPenTemplateQueryElement.is, SeaPenTemplateQueryElement);
