// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_progress/cr_progress.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './progress_indicator_demo.html.js';

class ProgressIndicatorDemoElement extends PolymerElement {
  static get is() {
    return 'progress-indicator-demo';
  }

  static get template() {
    return getTemplate();
  }
}

export const tagName = ProgressIndicatorDemoElement.is;

customElements.define(
    ProgressIndicatorDemoElement.is,
    ProgressIndicatorDemoElement);
