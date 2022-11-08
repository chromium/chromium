// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {getTemplate} from './touchpad_tester.html.js';

export interface TouchpadTesterElement {
  $: {touchpadTesterDialog: CrDialogElement};
}

const TouchpadTesterElementBase = I18nMixin(PolymerElement);

export class TouchpadTesterElement extends TouchpadTesterElementBase {
  static get is() {
    return 'touchpad-tester';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  // Touchpad device being tested.
  touchpad: TouchDeviceInfo|null = null;

  /**
   * Resets dialog configuration to default.
   */
  close(): void {
    this.$.touchpadTesterDialog.close();
    this.touchpad = null;
  }

  /** Setup display for requested touchpad.*/
  show(touchpad: TouchDeviceInfo): void {
    assert(!!touchpad);
    this.touchpad = touchpad;
    this.$.touchpadTesterDialog.showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'touchpad-tester': TouchpadTesterElement;
  }
}

customElements.define(TouchpadTesterElement.is, TouchpadTesterElement);
