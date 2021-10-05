/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class SupportToolElement extends PolymerElement {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      collectBrowserLogs_: {
        type: Boolean,
        value: true,
      },
      collectChromeOSLogs_: {
        type: Boolean,
        value() {
          return isChromeOS;
        },
      },
      hideChromeOS_: {
        type: Boolean,
        value() {
          return !isChromeOS;
        },
      }
    };
  }
}

customElements.define(SupportToolElement.is, SupportToolElement);