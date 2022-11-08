// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './touchpad_tester.html.js';

export interface TouchpadTesterElement {
  $: {touchpadTesterDialog: CrDialogElement};
}

export class TouchpadTesterElement extends PolymerElement {
  static get is() {
    return 'touchpad-tester';
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
    'touchpad-tester': TouchpadTesterElement;
  }
}

customElements.define(TouchpadTesterElement.is, TouchpadTesterElement);
