// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ui_trigger_list_object.html.js';


/** @polymer */
class UiTriggerObjectElement extends PolymerElement {
  static get is() {
    return 'ui-trigger-object';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * Sets the string representation of time.
   */
  private formatTime_(time: number): string {
    const d = new Date(time);
    return d.toLocaleTimeString();
  }
}

customElements.define(UiTriggerObjectElement.is, UiTriggerObjectElement);
