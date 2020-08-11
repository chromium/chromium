// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TabSearchAppElement} from 'chrome://tab-search/app.js';
import {TabSearchSearchField} from 'chrome://tab-search/tab_search_search_field.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from 'chrome://tab-search/tab_search_api_proxy.js'

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

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
    return tabSearchApp.shadowRoot.querySelectorAll('tab-search-item');
  }

  function sampleData() {
    const profileTabs = {
      windows: [
        {
          active: true,
          tabs: [
            {
              index: 0,
              tabId: 1,
              favIconUrl: '',
              title: 'Google',
              url: 'https://www.google.com',
            },
            {
              index: 1,
              tabId: 5,
              favIconUrl: '',
              title: 'Amazon',
              url: 'https://www.amazon.com',
            },
            {
              index: 2,
              tabId: 6,
              favIconUrl: '',
              title: 'Apple',
              url: 'https://www.apple.com',
            }
          ],
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

    return profileTabs;
  }

  async function setupTest(sampleData) {
    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileTabs(sampleData);
    TabSearchApiProxyImpl.instance_ = testProxy;

    tabSearchApp = /** @type {!TabSearchAppElement} */
        (document.createElement('tab-search-app'));

    document.body.innerHTML = '';
    document.body.appendChild(tabSearchApp);
    await flushTasks();
  }

  test('return all tabs', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
  });

  test('return filtered tabs', async () => {
    await setupTest(sampleData());
    const searchField = /** @type {!TabSearchSearchField} */
      (tabSearchApp.shadowRoot.querySelector("#searchField"));
    searchField.setValue('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [2]);
  });

  test('Default tab selection when data is present', async () => {
    await setupTest(sampleData());
    assertTrue(
        tabSearchApp.getSelectedIndex() != -1,
        'No default selection in the precense of data');
  });

  test('Click on tab item triggers actions', async () => {
    const tabData = {
      index: 0,
      tabId: 1,
      favIconUrl: '',
      title: 'Google',
      url: 'https://www.google.com',
    };
    await setupTest({windows: [{active: true, tabs: [tabData]}]});

    const tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('tab-search-item'));
    tabSearchItem.click();
    const [tabInfo] = await testProxy.whenCalled('switchToTab');
    assertEquals(tabData.tabId, tabInfo.tabId);

    const tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    tabSearchItemCloseButton.click();
    const tabId = await testProxy.whenCalled('closeTab');
    assertEquals(tabData.tabId, tabId);
  });

  test('Keyboard navigation on an empty list', async () => {
    await setupTest({windows: [{active: true, tabs: []}]});

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector("#searchField"));

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'Home');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'End');
    assertEquals(-1, tabSearchApp.getSelectedIndex());
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    await setupTest(sampleData());

    const numTabs =
        sampleData().windows.reduce((total, w) => total + w.tabs.length, 0);
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector("#searchField"));

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(numTabs - 1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(0, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(0, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'End');
    assertEquals(numTabs - 1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'Home');
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test('Key with modifiers should not affect selected item', async () => {
    await setupTest(sampleData());

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));

    for (const key of ['ArrowUp', 'ArrowDown', 'Home', 'End']) {
      keyDownOn(searchField, 0, ['shift'], key);
      assertEquals(0, tabSearchApp.getSelectedIndex());
    }
  });

  test('refresh on tabs changed', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    testProxy.setProfileTabs({windows: []});
    testProxy.getCallbackRouterRemote().tabsChanged();
    await flushTasks();
    verifyTabIds(queryRows(), []);
  });

  test('Verify initial tab render time is logged correctly.', async () => {
    // |recordTimeCalled| tracks the number of calls to recordTime().
    let recordTimeCalled = 0;
    // |metricString| tracks the metric name passed to recordTime().
    let metricName;
    chrome.metricsPrivate.recordTime = (...args) => {
      recordTimeCalled += 1;
      metricName = args[0];
    };

    await setupTest(sampleData());
    await waitAfterNextRender(tabSearchApp);

    // Make sure that tab data has been recieved.
    verifyTabIds(queryRows(), [ 1, 5, 6, 2, 3, 4 ]);

    // Ensure that |chrome.metricsPrivate.recordTime()| has been called
    // once after initial tab data has been recieved.
    assertEquals(1, recordTimeCalled);
    assertEquals('Tabs.TabSearch.WebUI.InitialTabsRenderTime', metricName);

    // Force a change to filtered tab data that would result in a
    // re-render.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    await waitAfterNextRender(tabSearchApp);
    verifyTabIds(queryRows(), [ 2 ]);

    // |chrome.metricsPrivate.recordTime()| should still have only been
    // called once.
    assertEquals(1, recordTimeCalled);
  });

  test('Verify tab switch is logged correctly.', async () => {
    await setupTest(sampleData());
    // Make sure that tab data has been recieved.
    verifyTabIds(queryRows(), [ 1, 5, 6, 2, 3, 4 ]);

    // Click the first element with tabId 1.
    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('tab-search-item[id="1"]'));
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    await testProxy.whenCalled('switchToTab')
        .then(([ tabInfo, withSearch ]) => {
          assertEquals(1, tabInfo.tabId);
          assertFalse(withSearch);
        });

    // Force a change to filtered tab data that would result in a
    // re-render.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [ 2 ]);

    testProxy.reset();
    // Click the only remaining element with tabId 2.
    tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('tab-search-item[id="2"]'));
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for a tab list fitlered by
    // the search query.
    await testProxy.whenCalled('switchToTab')
        .then(([ tabInfo, withSearch ]) => {
          assertEquals(2, tabInfo.tabId);
          assertTrue(withSearch);
        });
  });
});
