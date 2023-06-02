// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {CustomElement} from '//resources/js/custom_element.js';

import {getTemplate} from './progress_indicator_native_demo.html.js';

class ProgressIndicatorNativeDemoElement extends CustomElement {
  static get is() {
    return 'progress-indicator-native-demo';
  }

  static override get template() {
    return getTemplate();
  }
}

export const tagName = ProgressIndicatorNativeDemoElement.is;

customElements.define(
    ProgressIndicatorNativeDemoElement.is, ProgressIndicatorNativeDemoElement);
