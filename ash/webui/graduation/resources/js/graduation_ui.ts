// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './graduation_ui.html.js';

export class GraduationUi extends PolymerElement {
  static get is() {
    return 'graduation-ui';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(GraduationUi.is, GraduationUi);
