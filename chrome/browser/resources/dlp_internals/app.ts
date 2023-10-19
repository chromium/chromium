// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './dlp_tabs.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './app.html.js';

class DlpInternalsAppElement extends CustomElement {
  static get is() {
    return 'dlp-internals-app';
  }

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();

    const mainRootEl = this.$('#main-root');
    assert(mainRootEl);

    let rootClass = 'otr';
    if (!loadTimeData.getBoolean('isOtr')) {
      rootClass = 'valid-context';
      const tabsRoot = this.$('#tabs-root');
      assert(tabsRoot);
      let tabsElement;
      if (!loadTimeData.getBoolean('doRulesManagerExist')) {
        tabsElement = document.createTextNode(
            'The Rules Manager of Data Leak Prevention policy doesn\'t exist.');
      } else {
        tabsElement = document.createElement('dlp-tabs');
      }
      tabsRoot.appendChild(tabsElement);
    }

    mainRootEl.classList.add(rootClass);
  }
}

customElements.define(DlpInternalsAppElement.is, DlpInternalsAppElement);
