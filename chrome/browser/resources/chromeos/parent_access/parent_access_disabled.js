// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ParentAccessDisabled extends PolymerElement {
  static get is() {
    return 'parent-access-disabled';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @private */
  onDisabledScreenClosed_() {
    // TODO(b/266830608): Implement a Mojo interface for handling the disabled
    // state.
  }
}

customElements.define(ParentAccessDisabled.is, ParentAccessDisabled);
