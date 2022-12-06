// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './connectors_tabs.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './app.html.js';

class ConnectorsInternalsAppElement extends CustomElement {
  static get is() {
    return 'connectors-internals-app';
  }

  static override get template() {
    return getTemplate();
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
        const tabsElement = document.createElement('connectors-tabs');
        tabsRoot.appendChild(tabsElement);
      } else {
        console.error('Could not find tabs root.');
      }
    }
    mainRootEl.classList.add(rootClass);
  }
}

customElements.define(
    ConnectorsInternalsAppElement.is, ConnectorsInternalsAppElement);
