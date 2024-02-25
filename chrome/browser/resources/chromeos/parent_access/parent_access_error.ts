// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_template.js';
import './supervision/supervised_user_error.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './parent_access_error.html.js';

class ParentAccessError extends PolymerElement {
  static get is() {
    return 'parent-access-error';
  }

  static get template() {
    return getTemplate();
  }
}
customElements.define(ParentAccessError.is, ParentAccessError);
