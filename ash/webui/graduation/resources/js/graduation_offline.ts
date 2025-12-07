// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './graduation_offline.html.js';

class GraduationOffline extends PolymerElement {
  static get is() {
    return 'graduation-offline';
  }

  static get template() {
    return getTemplate();
  }
}
customElements.define(GraduationOffline.is, GraduationOffline);
