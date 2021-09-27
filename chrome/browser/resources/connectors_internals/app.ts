// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './zero_trust_connector.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ConnectorsInternalsAppElement extends PolymerElement {
  static get is() {
    return 'connectors-internals-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      isOtr: Boolean,
    };
  }

  public readonly isOtr: boolean;

  constructor() {
    super();

    this.isOtr = loadTimeData.getBoolean('isOtr');
  }
}

customElements.define(
    ConnectorsInternalsAppElement.is, ConnectorsInternalsAppElement);