// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-subsection-header' displays information about a device, with
 * conditional layout based on the 'isWelcomeExperienceEnabled' flag.
 * - When enabled: Shows device image (if available), name, and optional battery
 * info.
 * - When disabled: Shows device name only.
 */

import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './per_device_subsection_header.html.js';

export class PerDeviceSubsectionHeaderElement extends PolymerElement {
  static get is() {
    return 'per-device-subsection-header' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerDeviceSubsectionHeaderElement.is]: PerDeviceSubsectionHeaderElement;
  }
}

customElements.define(
    PerDeviceSubsectionHeaderElement.is, PerDeviceSubsectionHeaderElement);
