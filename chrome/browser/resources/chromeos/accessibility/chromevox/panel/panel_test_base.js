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

    // Alphabetical based on file path.
    await importModule(
        ['PanelCommand', 'PanelCommandType'],
        '/chromevox/common/panel_command.js');

    await new PanelCommand(PanelCommandType.ENABLE_TEST_HOOKS).send();
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
};
