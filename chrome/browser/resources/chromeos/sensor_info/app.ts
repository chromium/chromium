// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class SensorInfoAppElement extends PolymerElement {
  static get is() {
    return 'sensor-info-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      message_: {
        type: String,
        value: 'Sensor Info',
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sensor-info-app': SensorInfoAppElement;
  }
}


customElements.define(SensorInfoAppElement.is, SensorInfoAppElement);
