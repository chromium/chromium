// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Base class for Panel tests.
 */
ChromeVoxPanelTestBase = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await this.waitForPendingMethods();
    await PanelBridge.disableMessagesForTest();
  }


  async isMenuTitleMessage(menuTitleMessage) {
    const response = await PanelBridge.getActiveMenuDataForTest()

    return menuTitleMessage === response.menuMsg;
  }

  async waitForMenu(menuTitleMessage) {
    // TODO(crbug.com/424764877): Replace polling.
    let pollForMenu = async (resolve) => {
      if (await this.isMenuTitleMessage(menuTitleMessage)) {
        resolve();
      } else {
        setTimeout(() => pollForMenu(resolve), 500)
      }
    };
    return new Promise(resolve => {
      pollForMenu(resolve);
    });
  }
};
