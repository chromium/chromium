// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays input for SeaPen wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {PersonalizationRouterElement} from '../../personalization_router_element.js';
import {WithPersonalizationStore} from '../../personalization_store.js';
import {QUERY} from '../utils.js';

import {getTemplate} from './sea_pen_input_element.html.js';

export class SeaPenInputElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-input';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      textValue_: String,
    };
  }
  private textValue_: string;

  private onClickSearchButton_() {
    assert(this.textValue_, 'input text should not be empty.');
    PersonalizationRouterElement.instance().selectSeaPenTemplate(QUERY);
    // TODO(b/299107965): add and call mocked api to search for thumbnails.
  }
}
customElements.define(SeaPenInputElement.is, SeaPenInputElement);
