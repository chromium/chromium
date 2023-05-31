// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './progress_indicator_polymer_demo.html.js';

class ProgressIndicatorPolymerDemoElement extends PolymerElement {
  static get is() {
    return 'progress-indicator-polymer-demo';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    ProgressIndicatorPolymerDemoElement.is,
    ProgressIndicatorPolymerDemoElement);
