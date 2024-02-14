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

    await new PanelCommand(PanelCommandType.ENABLE_TEST_HOOKS).send();
    await this.waitForPendingMethods();
    this.getPanelWindow().MenuManager.disableMissingMsgsErrorsForTesting = true;
  }

  getPanelWindow() {
    let panelWindow = null;
    while (!panelWindow) {
      panelWindow = chrome.extension.getViews().find(
          view => view.location.href.indexOf('chromevox/panel/panel.html') > 0);
    }
    return panelWindow;
  }

  /**
   * Gets the Panel object in the panel.html window. Note that the extension
   * system destroys our reference to this object unpredictably so always ask
   * chrome.extension.getViews for it.
   */
  getPanel() {
    return this.getPanelWindow().Panel;
  }

  async waitForMenu(menuMsg) {
    const menuManager = this.getPanel().instance.menuManager_;

    // Menu and menu item updates occur in a different js context, so tests need
    // to wait until an update has been made.
    return new Promise(
        resolve =>
            this.addCallbackPostMethod(menuManager, 'activateMenu', () => {
              assertEquals(menuMsg, menuManager.activeMenu_.menuMsg);
              resolve();
            }, () => true));
  }
};
