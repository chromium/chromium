// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays the Ambient zero state.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './zero_state_element.html.js';

export class AmbientZeroStateElement extends WithPersonalizationStore {
  static get is() {
    return 'ambient-zero-state';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** Whether the page is being rendered in dark mode. */
  private isDarkModeActive_: boolean;

  /**
   * Returns the image source based on whether the page is being
   * rendered in dark mode.
   */
  private getImageSource_() {
    return this.isDarkModeActive_ ?
        'chrome://personalization/images/ambient_mode_disabled_dark.svg' :
        'chrome://personalization/images/ambient_mode_disabled.svg';
  }
}

customElements.define(AmbientZeroStateElement.is, AmbientZeroStateElement);
