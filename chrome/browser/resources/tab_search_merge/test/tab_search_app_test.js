// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabSearchAppElement} from 'chrome://tab-search/app.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from 'chrome://tab-search/tab_search_api_proxy.js'

import {assertEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchAppTest', () => {
  /** @type {!TabSearchAppElement} */
  let tabSearchApp;
  /** @type {!TestTabSearchApiProxy} */
  let testProxy;

  /**
   * @param {!NodeList<!Element>} rows
   * @param {!Array<number>} ids
   */
  function verifyTabIds(rows, ids) {
    assertEquals(ids.length, rows.length);
    rows.forEach((row, index) => {
      assertEquals(ids[index].toString(), row.getAttribute('id'));
    });
  }

  /**
   * @return {!NodeList<!Element>}
   */
  function queryRows() {
    return tabSearchApp.shadowRoot.querySelectorAll('.row');
  }

  function setupData() {
    const profileTabs = {
      windows: [
        {
          active: true,
          tabs: [{
            index: 0,
            tabId: 1,
            favIconUrl: '',
            title: 'Google',
            url: 'https://www.google.com',
          }],
        },
        {
          active: false,
          tabs: [
            {
              index: 0,
              tabId: 2,
              favIconUrl: '',
              title: 'Bing',
              url: 'https://www.bing.com/',
            },
            {
              index: 1,
              tabId: 3,
              favIconUrl: '',
              title: 'Yahoo',
              url: 'https://www.yahoo.com',
            },
            {
              index: 2,
              tabId: 4,
              favIconUrl: '',
              title: 'Apple',
              url: 'https://www.apple.com/',
            },
          ]
        }
      ]
    };

    testProxy.setProfileTabs(profileTabs);
  }

  setup(() => {
    testProxy = new TestTabSearchApiProxy();
    TabSearchApiProxyImpl.instance_ = testProxy;
    setupData();

    tabSearchApp = /** @type {!TabSearchAppElement} */
        (document.createElement('tab-search-app'));
    document.body.appendChild(tabSearchApp);
  });

  test('return all tabs', async () => {
    await flushTasks();
    verifyTabIds(queryRows(), [1, 2, 3, 4]);
  });

  test('return filtered tabs', async () => {
    tabSearchApp.setSearchText('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [2]);
  });
});
