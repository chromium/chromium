// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './extension_approvals_disabled.html.js';

export class ExtensionApprovalsDisabled extends PolymerElement {
  static get is() {
    return 'extension-approvals-disabled';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    ExtensionApprovalsDisabled.is, ExtensionApprovalsDisabled);
