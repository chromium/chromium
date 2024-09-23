// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['panel_test_base.js']);

/**
 * Test fixture for Panel.
 */
ChromeVoxPanelTest = class extends ChromeVoxPanelTestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`#include "ui/accessibility/accessibility_features.h"`);
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.Gesture = chrome.accessibilityPrivate.Gesture;
    globalThis.RoleType = chrome.automation.RoleType;

    const panel = this.getPanel().instance;
    const original = panel.exec_.bind(panel);
    panel.exec_ = (command) => {
      original(command);
      this.onPanelCommandCalled();
    };
  }

  onPanelCommandCalled() {
    if (this.resolvePanelCommandPromise) {
      this.resolvePanelCommandPromise();
    }
  }

  prepareForPanelCommand() {
    this.panelCommandPromise =
        new Promise(resolve => this.resolvePanelCommandPromise = resolve);
  }

  waitForPanelCommand() {
    return this.panelCommandPromise;
  }

  fireMockEvent(key) {
    return function() {
      const obj = {};
      obj.preventDefault = function() {};
      obj.stopPropagation = function() {};
      obj.key = key;
      this.getPanel().instance.onKeyDown_(obj);
    }.bind(this);
  }

  fireMockQuery(query) {
    return function() {
      const evt = {};
      evt.target = {};
      evt.target.value = query;
      this.getPanel().instance.menuManager_.onSearchBarQuery(evt);
    }.bind(this);
  }

  assertActiveMenuItem(menuMsg, menuItemTitle, opt_menuItemShortcut) {
    const menu = this.getPanel().instance.menuManager_.activeMenu_;
    const menuItem = menu.items_[menu.activeIndex_];
    assertEquals(menuMsg, menu.menuMsg);
    assertEquals(menuItemTitle, menuItem.menuItemTitle);
    if (opt_menuItemShortcut) {
      assertEquals(opt_menuItemShortcut, menuItem.menuItemShortcut);
    }
  }

  assertActiveSearchMenuItem(menuItemTitle) {
    const searchMenu = this.getPanel().instance.menuManager_.searchMenu;
    const activeIndex = searchMenu.activeIndex_;
    const activeItem = searchMenu.items_[activeIndex];
    assertEquals(menuItemTitle, activeItem.menuItemTitle);
  }

  enableTouchMode() {
    EventSource.set(EventSourceType.TOUCH_GESTURE);
  }

  isMenuTitleMessage(menuTitleMessage) {
    const menu = this.getPanel().instance.menuManager_.activeMenu_;
    return menuTitleMessage === menu.menuMsg;
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

AX_TEST_F('ChromeVoxPanelTest', 'ActivateMenu', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');
  this.fireMockEvent('ArrowRight')();
  this.assertActiveMenuItem('panel_menu_jump', 'Go To Beginning Of Table');
  this.fireMockEvent('ArrowRight')();
  this.assertActiveMenuItem(
      'panel_menu_speech', 'Announce Current Battery Status');
});

// TODO(https://crbug.com/1299765): Re-enable once flaky timeouts are fixed.
AX_TEST_F('ChromeVoxPanelTest', 'DISABLED_LinkMenu', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  CommandHandlerInterface.instance.onCommand('showLinksList');
  await this.waitForMenu('role_link');
  this.fireMockEvent('ArrowLeft')();
  this.assertActiveMenuItem('role_landmark', 'No items');
  this.fireMockEvent('ArrowRight')();
  this.assertActiveMenuItem('role_link', 'apple Internal link');
  this.fireMockEvent('ArrowUp')();
  this.assertActiveMenuItem('role_link', 'banana Internal link');
});

AX_TEST_F('ChromeVoxPanelTest', 'FormControlsMenu', async function() {
  await this.runWithLoadedTree(`<button>Cancel</button><button>OK</button>`);
  CommandHandlerInterface.instance.onCommand('showFormsList');
  await this.waitForMenu('panel_menu_form_controls');
  this.fireMockEvent('ArrowDown')();
  this.assertActiveMenuItem('panel_menu_form_controls', 'OK Button');
  this.fireMockEvent('ArrowUp')();
  this.assertActiveMenuItem('panel_menu_form_controls', 'Cancel Button');
});


// TODO(https://crbug.com/1333375): Flaky on MSAN and ASAN builders.
GEN('#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER)');
GEN('#define MAYBE_SearchMenu DISABLED_SearchMenu');
GEN('#else');
GEN('#define MAYBE_SearchMenu SearchMenu');
GEN('#endif');

AX_TEST_F('ChromeVoxPanelTest', 'MAYBE_SearchMenu', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.linksDoc);
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');
  await mockFeedback
      .expectSpeech('Search the menus', /Type to search the menus/)
      .call(() => {
        this.fireMockQuery('jump')();
        this.assertActiveSearchMenuItem('Jump To Details');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(() => {
        this.fireMockEvent('ArrowDown')();
        this.assertActiveSearchMenuItem('Jump To The Bottom Of The Page');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(() => {
        this.fireMockEvent('ArrowDown')();
        this.assertActiveSearchMenuItem('Jump To The Top Of The Page');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(() => {
        this.fireMockEvent('ArrowDown')();
        this.assertActiveSearchMenuItem('Jump To Details');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .replay();
});

// TODO(crbug.com/1088438): flaky crashes.
AX_TEST_F('ChromeVoxPanelTest', 'DISABLED_Gestures', async function() {
  const doGestureAsync = async gesture => {
    doGesture(gesture)();
  };
  await this.runWithLoadedTree(`<button>Cancel</button><button>OK</button>`);
  doGestureAsync(Gesture.TAP4);
  await this.waitForMenu('panel_search_menu');
  // GestureCommandHandler behaves in special ways only with range over
  // the panel. Fake this out by setting range there.
  const desktop = root.parent.root;
  const panelNode = desktop.find(
      {role: 'rootWebArea', attributes: {name: 'ChromeVox Panel'}});
  ChromeVoxRange.set(CursorRange.fromNode(panelNode));

  doGestureAsync(Gesture.SWIPE_RIGHT1);
  await this.waitForMenu('panel_menu_jump');

  doGestureAsync(Gesture.SWIPE_RIGHT1);
  await this.waitForMenu('panel_menu_speech');

  doGestureAsync(Gesture.SWIPE_LEFT1);
  await this.waitForMenu('panel_menu_jump');
});

AX_TEST_F(
    'ChromeVoxPanelTest', 'InternationalFormControlsMenu', async function() {
      await this.runWithLoadedTree(this.internationalButtonDoc);
      // Turn on language switching and set available voice list.
      SettingsManager.set('languageSwitching', true);
      LocaleOutputHelper.instance.availableVoices_ =
          [{'lang': 'en-US'}, {'lang': 'es-ES'}];
      CommandHandlerInterface.instance.onCommand('showFormsList');
      await this.waitForMenu('panel_menu_form_controls');
      this.fireMockEvent('ArrowDown')();
      this.assertActiveMenuItem(
          'panel_menu_form_controls', 'español: Prueba Button');
      this.fireMockEvent('ArrowUp')();
      this.assertActiveMenuItem('panel_menu_form_controls', 'Test Button');
    });

AX_TEST_F('ChromeVoxPanelTest', 'ActionsMenu', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  CommandHandlerInterface.instance.onCommand('showActionsMenu');
  await this.waitForMenu('panel_menu_actions');
  this.fireMockEvent('ArrowDown')();
  this.assertActiveMenuItem('panel_menu_actions', 'Start Or End Selection');
  this.fireMockEvent('ArrowUp')();
  this.assertActiveMenuItem('panel_menu_actions', 'Click On Current Item');
});

AX_TEST_F('ChromeVoxPanelTest', 'ActionsMenuLongClick', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  // Get the node that will be checked for actions.
  const activeNode = ChromeVoxRange.current.start.node;
  // Override the standard actions to have long click.
  Object.defineProperty(activeNode, 'standardActions', {
    value: ['longClick'],
    writable: true,
  });
  CommandHandlerInterface.instance.onCommand('showActionsMenu');
  await this.waitForMenu('panel_menu_actions');
  // Go down three times
  this.fireMockEvent('ArrowUp')();
  this.assertActiveMenuItem('panel_menu_actions', 'Long click on current item');
  this.fireMockEvent('ArrowDown')();
  this.assertActiveMenuItem('panel_menu_actions', 'Click On Current Item');
});

AX_TEST_F(
    'ChromeVoxPanelTest', 'ShortcutsAreInternationalized', async function() {
      await this.runWithLoadedTree(this.linksDoc);
      new PanelCommand(PanelCommandType.OPEN_MENUS).send();
      await this.waitForMenu('panel_search_menu');
      this.fireMockEvent('ArrowRight')();
      this.assertActiveMenuItem(
          'panel_menu_jump', 'Go To Beginning Of Table',
          'Search+Alt+Shift+ArrowLeft');
      this.fireMockEvent('ArrowRight')();
      this.assertActiveMenuItem(
          'panel_menu_speech', 'Announce Current Battery Status',
          'Search+O, then B');
      this.fireMockEvent('ArrowRight')();
      this.assertActiveMenuItem(
          'panel_menu_chromevox', 'Enable/Disable Sticky Mode',
          'Search+Search');
    });

// Ensure 'Touch Gestures' is not in the panel menus by default.
AX_TEST_F(
    'ChromeVoxPanelTest', 'TouchGesturesMenuNotAvailableWhenNotInTouchMode',
    async function() {
      await this.runWithLoadedTree(this.linksDoc);
      new PanelCommand(PanelCommandType.OPEN_MENUS).send();
      await this.waitForMenu('panel_search_menu');
      do {
        this.fireMockEvent('ArrowRight')();
        assertFalse(this.isMenuTitleMessage('panel_menu_touchgestures'));
      } while (!this.isMenuTitleMessage('panel_search_menu'));
    });

// Ensure 'Touch Gesture' is in the panel menus when touch mode is enabled.
AX_TEST_F(
    'ChromeVoxPanelTest', 'TouchGesturesMenuAvailableWhenInTouchMode',
    async function() {
      await this.runWithLoadedTree(this.linksDoc);
      this.enableTouchMode();
      new PanelCommand(PanelCommandType.OPEN_MENUS).send();
      await this.waitForMenu('panel_search_menu');

      // Look for Touch Gestures menu, fail if getting back to start.
      do {
        this.fireMockEvent('ArrowRight')();
        assertFalse(this.isMenuTitleMessage('panel_search_menu'));
      } while (!this.isMenuTitleMessage('panel_menu_touchgestures'));

      this.assertActiveMenuItem(
          'panel_menu_touchgestures', 'Click on current item');
    });

// Ensure 'Perform default action' in the actions tab invokes a click event.
AX_TEST_F('ChromeVoxPanelTest', 'PerformDoDefaultAction', async function() {
  const rootNode = await this.runWithLoadedTree(`<button>OK</button>`);
  const button = rootNode.find({role: RoleType.BUTTON});
  await this.waitForEvent(button, chrome.automation.EventType.FOCUS);
  CommandHandlerInterface.instance.onCommand('showActionsMenu');
  await this.waitForMenu('panel_menu_actions');
  this.fireMockEvent('ArrowDown')();
  this.assertActiveMenuItem('panel_menu_actions', 'Start Or End Selection');
  this.fireMockEvent('ArrowDown')();
  this.fireMockEvent('ArrowDown')();
  this.assertActiveMenuItem('panel_menu_actions', 'Perform default action');
  this.fireMockEvent('Enter')();
  await this.waitForEvent(button, chrome.automation.EventType.CLICKED);
});

AX_TEST_F('ChromeVoxPanelTest', 'PanVirtualBrailleDisplay', async function() {
  await this.runWithLoadedTree(this.linksDoc);

  this.prepareForPanelCommand();
  CommandHandlerInterface.instance.onCommand('toggleBrailleCaptions');
  this.waitForPanelCommand();

  // Locate the buttons to pan left and pan right in the display.
  const panelDocument = this.getPanelWindow().document;
  const panLeftButton = panelDocument.getElementById('braille-pan-left');
  assertNotNullNorUndefined(panLeftButton);
  const panRightButton = panelDocument.getElementById('braille-pan-right');
  assertNotNullNorUndefined(panRightButton);

  // Mock out ChromeVox.braille to confirm that the commands are routed from the
  // panel context to the background context.
  let panLeft;
  let panRight;
  const panLeftDone = new Promise(resolve => panLeft = resolve);
  const panRightDone = new Promise(resolve => panRight = resolve);
  ChromeVox.braille = {panLeft, panRight};

  panLeftButton.click();
  await panLeftDone;

  panRightButton.click();
  await panRightDone;
});
