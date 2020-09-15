// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js'
]);

/**
 * Test fixture for Panel.
 */
ChromeVoxPanelTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  testGenCppIncludes() {
    ChromeVoxE2ETest.prototype.testGenCppIncludes.call(this);
  }

  getPanelWindow() {
    let panelWindow = null;
    while (!panelWindow) {
      panelWindow = chrome.extension.getViews().find(function(view) {
        return view.location.href.indexOf('chromevox/panel/panel.html') > 0;
      });
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

  fireMockEvent(key) {
    return function() {
      const obj = {};
      obj.preventDefault = function() {};
      obj.stopPropagation = function() {};
      obj.key = key;
      this.getPanel().onKeyDown(obj);
    }.bind(this);
  }

  fireMockQuery(query) {
    return function() {
      const evt = {};
      evt.target = {};
      evt.target.value = query;
      this.getPanel().onSearchBarQuery(evt);
    }.bind(this);
  }

  async waitForMenu(menuMsg) {
    // Menu and menu item updates occur in a different js context, so tests need
    // to wait until an update has been made. Swap in our hook, wait, then
    // restore after.
    const makeAssertions = () => {
      const menu = this.getPanel().activeMenu_;
      assertEquals(menuMsg, menu.menuMsg);
    };

    return new Promise(resolve => {
      const Panel = this.getPanel();
      const original = Panel.activateMenu;
      Panel.activateMenu = (menu, activateFirstItem) => {
        original(menu, activateFirstItem);
        makeAssertions();
        Panel.activateMenu = original;
        resolve();
      };
    });
  }

  assertActiveMenuItem(menuMsg, menuItemTitle) {
    const menu = this.getPanel().activeMenu_;
    const menuItem = menu.items_[menu.activeIndex_];
    assertEquals(menuMsg, menu.menuMsg);
    assertEquals(menuItemTitle, menuItem.menuItemTitle);
  }

  assertActiveSearchMenuItem(menuItemTitle) {
    const searchMenu = this.getPanel().searchMenu;
    const activeIndex = searchMenu.activeIndex_;
    const activeItem = searchMenu.items_[activeIndex];
    assertEquals(menuItemTitle, activeItem.menuItemTitle);
  }

  get linksDoc() {
    return `
      <p>start</p>
      <a href="#">apple</a>
      <a href="#">grape</a>
      <a href="#">banana</a>
    `;
  }

  get internationalButtonDoc() {
    return `
      <button lang="en">Test</button>
      <button lang="es">Prueba</button>
    `;
  }
};

TEST_F('ChromeVoxPanelTest', 'ActivateMenu', function() {
  this.runWithLoadedTree(this.linksDoc, async function(root) {
    new PanelCommand(PanelCommandType.OPEN_MENUS).send();
    await this.waitForMenu('panel_search_menu');
    this.fireMockEvent('ArrowRight')();
    this.assertActiveMenuItem('panel_menu_jump', 'Go To Beginning Of Table');
    this.fireMockEvent('ArrowRight')();
    this.assertActiveMenuItem(
        'panel_menu_speech', 'Announce Current Battery Status');
  });
});

TEST_F('ChromeVoxPanelTest', 'LinkMenu', function() {
  this.runWithLoadedTree(this.linksDoc, async function(root) {
    CommandHandler.onCommand('showLinksList');
    await this.waitForMenu('role_link');
    this.fireMockEvent('ArrowLeft')();
    this.assertActiveMenuItem('role_landmark', 'No items');
    this.fireMockEvent('ArrowRight')();
    this.assertActiveMenuItem('role_link', 'apple Internal link');
    this.fireMockEvent('ArrowUp')();
    this.assertActiveMenuItem('role_link', 'banana Internal link');
  });
});

TEST_F('ChromeVoxPanelTest', 'FormControlsMenu', function() {
  this.runWithLoadedTree(
      `<button>Cancel</button><button>OK</button>`, async function(root) {
        CommandHandler.onCommand('showFormsList');
        await this.waitForMenu('panel_menu_form_controls');
        this.fireMockEvent('ArrowDown')();
        this.assertActiveMenuItem('panel_menu_form_controls', 'OK Button');
        this.fireMockEvent('ArrowUp')();
        this.assertActiveMenuItem('panel_menu_form_controls', 'Cancel Button');
      });
});

TEST_F('ChromeVoxPanelTest', 'SearchMenu', function() {
  this.runWithLoadedTree(this.linksDoc, async function(root) {
    new PanelCommand(PanelCommandType.OPEN_MENUS).send();
    await this.waitForMenu('panel_search_menu');
    this.fireMockQuery('jump')();
    this.assertActiveSearchMenuItem('Jump To Details');
    this.fireMockEvent('ArrowDown')();
    this.assertActiveSearchMenuItem('Jump To The Bottom Of The Page');
    this.fireMockEvent('ArrowDown')();
    this.assertActiveSearchMenuItem('Jump To The Top Of The Page');
    this.fireMockEvent('ArrowDown')();
    this.assertActiveSearchMenuItem('Jump To Details');
  });
});

// TODO(crbug.com/1088438): flaky crashes.
TEST_F('ChromeVoxPanelTest', 'DISABLED_Gestures', function() {
  const doGesture = async (gesture) => {
    GestureCommandHandler.onAccessibilityGesture_(gesture);
  };
  this.runWithLoadedTree(
      `<button>Cancel</button><button>OK</button>`, async function(root) {
        doGesture('tap4');
        await this.waitForMenu('panel_search_menu');
        // GestureCommandHandler behaves in special ways only with range over
        // the panel. Fake this out by setting range there.
        const desktop = root.parent.root;
        const panelNode = desktop.find(
            {role: 'rootWebArea', attributes: {name: 'ChromeVox Panel'}});
        ChromeVoxState.instance.setCurrentRange(
            cursors.Range.fromNode(panelNode));

        doGesture('swipeRight1');
        await this.waitForMenu('panel_menu_jump');

        doGesture('swipeRight1');
        await this.waitForMenu('panel_menu_speech');

        doGesture('swipeLeft1');
        await this.waitForMenu('panel_menu_jump');
      });
});

TEST_F('ChromeVoxPanelTest', 'InternationalFormControlsMenu', function() {
  this.runWithLoadedTree(this.internationalButtonDoc, async function(root) {
    // Turn on language switching and set available voice list.
    localStorage['languageSwitching'] = 'true';
    this.getPanelWindow().LocaleOutputHelper.instance.availableVoices_ =
        [{'lang': 'en-US'}, {'lang': 'es-ES'}];
    CommandHandler.onCommand('showFormsList');
    await this.waitForMenu('panel_menu_form_controls');
    this.fireMockEvent('ArrowDown')();
    this.assertActiveMenuItem(
        'panel_menu_form_controls', 'español: Prueba Button');
    this.fireMockEvent('ArrowUp')();
    this.assertActiveMenuItem(
        'panel_menu_form_controls', 'English: Test Button');
  });
});
