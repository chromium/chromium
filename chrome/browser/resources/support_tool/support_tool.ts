/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
import './strings.m.js';

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './support_tool.html.js';

export class SupportToolElement extends PolymerElement {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      caseId_: String,
    };
  }

  private caseId_: string = loadTimeData.getString('caseId');
}

customElements.define(SupportToolElement.is, SupportToolElement);