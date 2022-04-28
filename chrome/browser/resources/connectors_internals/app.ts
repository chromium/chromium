// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import {ConnectorsTabsElement} from './connectors_tabs.js';

class ConnectorsInternalsAppElement extends CustomElement {
  static get is() {
    return 'connectors-internals-app';
  }

  static override get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  constructor() {
    super();

    const mainRootEl = this.$('#main-root');
    if (!mainRootEl) {
      console.error('Could not find main root.');
      return;
    }

    let rootClass = 'otr';
    if (!loadTimeData.getBoolean('isOtr')) {
      rootClass = 'valid-context';
      const tabsRoot = this.$('#tabs-root');
      if (tabsRoot) {
        tabsRoot.innerHTML =
            `<${ConnectorsTabsElement.is}></${ConnectorsTabsElement.is}>`;
      } else {
        console.error('Could not find tabs root.');
      }
    }
    mainRootEl.classList.add(rootClass);
  }
}

customElements.define(
    ConnectorsInternalsAppElement.is, ConnectorsInternalsAppElement);
