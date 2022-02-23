/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './support_tool.html.js';

export class SupportToolElement extends PolymerElement {
  static get is() {
    return 'support-tool';
  }

  // TODO(crbug.com/1292025): Use html_to_wrapper() for getting the template.
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      collectBrowserLogs_: Boolean,
      collectChromeOSLogs_: Boolean,
      hideChromeOS_: Boolean,
    };
  }

  private collectBrowserLogs_: boolean = true;
  private collectChromeOSLogs_: boolean = isChromeOS;
  private hideChromeOS_: boolean = !isChromeOS;
}

customElements.define(SupportToolElement.is, SupportToolElement);