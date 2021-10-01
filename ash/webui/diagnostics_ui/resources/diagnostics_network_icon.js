// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'diagnostics-network-icon' is a wrapper for 'network-icon' to ensure the
 * correct icon displayed based on network type, state, and technology.
 * @see //ui/webui/resources/cr_components/chromeos/network/network_icon.js
 */
export class DiagnosticsNetworkIconElement extends PolymerElement {
  static get is() {
    return 'diagnostics-network-icon';
  }

  static get template() {
    return html`{__html_template__}`
  }
}

customElements.define(
    DiagnosticsNetworkIconElement.is, DiagnosticsNetworkIconElement);
