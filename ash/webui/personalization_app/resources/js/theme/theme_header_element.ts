// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the theme header and a toggle button.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './theme_header_element.html.js';

export class ThemeHeader extends WithPersonalizationStore {
  static get is() {
    return 'theme-header';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: {
        type: Boolean,
        value: true,
        notify: true,
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
}

customElements.define(ThemeHeader.is, ThemeHeader);
