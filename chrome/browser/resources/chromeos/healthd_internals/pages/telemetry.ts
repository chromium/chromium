// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './telemetry.html.js';

export class HealthdInternalsTelemetryElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-telemetry';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-telemetry': HealthdInternalsTelemetryElement;
  }
}

customElements.define(
    HealthdInternalsTelemetryElement.is, HealthdInternalsTelemetryElement);
