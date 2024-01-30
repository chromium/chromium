// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {getTemplate} from './vc_tray_tester.html.js';

/**
 * @fileoverview
 * 'vc-tray-tester' defines the UI for the "VC Tray Tester" test page.
 */

class VcTrayTesterElement extends HTMLElement {
  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }
}

customElements.define('vc-tray-tester', VcTrayTesterElement);
