// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './firmware_confirmation_dialog.js';
import './firmware_update_dialog.js';
import './peripheral_updates_list.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'firmware-update-app' is the main landing page for the firmware
 * update app.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FirmwareUpdateAppElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class FirmwareUpdateAppElement extends FirmwareUpdateAppElementBase {
  static get is() {
    return 'firmware-update-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(FirmwareUpdateAppElement.is, FirmwareUpdateAppElement);