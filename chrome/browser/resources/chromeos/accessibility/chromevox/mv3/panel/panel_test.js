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
  }

  fireMockEvent(key) {
    PanelBridge.fireMockEventForTest(key);
  }

  fireMockQuery(query) {
    PanelBridge.fireMockQueryForTest(query);
  }

  async assertActiveMenuItem(menuMsg, menuItemTitle, opt_menuItemShortcut) {
    const response = await PanelBridge.getActiveMenuDataForTest()

    assertEquals(menuMsg, response.menuMsg);
    assertEquals(menuItemTitle, response.menuItemTitle);
    if (opt_menuItemShortcut) {
      assertEquals(opt_menuItemShortcut, response.menuItemShortcut);
    }
  }

  async assertActiveSearchMenuItem(menuItemTitle) {
    const response = await PanelBridge.getActiveSearchMenuDataForTest();
    assertEquals(menuItemTitle, response.menuItemTitle);
  }

  enableTouchMode() {
    EventSource.set(EventSourceType.TOUCH_GESTURE);
  }

  braillePanRight() {
    PanelBridge.braillePanRightForTest();
  }

  braillePanLeft() {
    PanelBridge.braillePanLeftForTest();
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
  this.fireMockEvent('ArrowRight');
  await await this.assertActiveMenuItem(
      'panel_menu_jump', 'Go To Beginning Of Table');
  this.fireMockEvent('ArrowRight');
  await this.assertActiveMenuItem(
      'panel_menu_speech', 'Announce Current Battery Status');
});

AX_TEST_F('ChromeVoxPanelTest', 'LinkMenu', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  CommandHandlerInterface.instance.onCommand('showLinksList');
  await this.waitForMenu('role_link');
  this.fireMockEvent('ArrowLeft');
  await this.assertActiveMenuItem('role_landmark', 'No items');
  this.fireMockEvent('ArrowRight');
  await this.assertActiveMenuItem('role_link', 'apple Internal link');
  this.fireMockEvent('ArrowUp');
  await this.assertActiveMenuItem('role_link', 'banana Internal link');
});

AX_TEST_F('ChromeVoxPanelTest', 'FormControlsMenu', async function() {
  await this.runWithLoadedTree(`<button>Cancel</button><button>OK</button>`);
  CommandHandlerInterface.instance.onCommand('showFormsList');
  await this.waitForMenu('panel_menu_form_controls');
  this.fireMockEvent('ArrowDown');
  await this.assertActiveMenuItem('panel_menu_form_controls', 'OK Button');
  this.fireMockEvent('ArrowUp');
  await this.assertActiveMenuItem('panel_menu_form_controls', 'Cancel Button');
});


AX_TEST_F('ChromeVoxPanelTest', 'SearchMenu', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.linksDoc);
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  await mockFeedback
      .expectSpeech('Search the menus', /Type to search the menus/)
      .call(async () => {
        this.fireMockQuery('jump');
        await this.assertActiveSearchMenuItem('Jump To Details');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(async () => {
        this.fireMockEvent('ArrowDown');
        await this.assertActiveSearchMenuItem('Jump To The Bottom Of The Page');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(async () => {
        this.fireMockEvent('ArrowDown');
        await this.assertActiveSearchMenuItem('Jump To The Top Of The Page');
      })
      .expectSpeech(/Jump/, 'Menu item', /[0-9]+ of [0-9]+/)
      .call(async () => {
        this.fireMockEvent('ArrowDown');
        await this.assertActiveSearchMenuItem('Jump To Details');
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
      this.fireMockEvent('ArrowDown');
      await this.assertActiveMenuItem(
          'panel_menu_form_controls', 'español: Prueba Button');
      this.fireMockEvent('ArrowUp');
      await this.assertActiveMenuItem(
          'panel_menu_form_controls', 'Test Button');
    });

AX_TEST_F('ChromeVoxPanelTest', 'ActionsMenu', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  CommandHandlerInterface.instance.onCommand('showActionsMenu');
  await this.waitForMenu('panel_menu_actions');
  this.fireMockEvent('ArrowDown');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Start Or End Selection');
  this.fireMockEvent('ArrowUp');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Click On Current Item');
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
  this.fireMockEvent('ArrowUp');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Long click on current item');
  this.fireMockEvent('ArrowDown');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Click On Current Item');
});

AX_TEST_F(
    'ChromeVoxPanelTest', 'ShortcutsAreInternationalized', async function() {
      await this.runWithLoadedTree(this.linksDoc);
      new PanelCommand(PanelCommandType.OPEN_MENUS).send();
      await this.waitForMenu('panel_search_menu');
      this.fireMockEvent('ArrowRight');
      await this.assertActiveMenuItem(
          'panel_menu_jump', 'Go To Beginning Of Table',
          'Search+Alt+Shift+ArrowLeft');
      this.fireMockEvent('ArrowRight');
      await this.assertActiveMenuItem(
          'panel_menu_speech', 'Announce Current Battery Status',
          'Search+O, then B');
      this.fireMockEvent('ArrowRight');
      await this.assertActiveMenuItem(
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
        this.fireMockEvent('ArrowRight');
        assertFalse(await this.isMenuTitleMessage('panel_menu_touchgestures'));
      } while (!await this.isMenuTitleMessage('panel_search_menu'));
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
        this.fireMockEvent('ArrowRight');
        assertFalse(await this.isMenuTitleMessage('panel_search_menu'));
      } while (!await this.isMenuTitleMessage('panel_menu_touchgestures'));

      await this.assertActiveMenuItem(
          'panel_menu_touchgestures', 'Click on current item');
    });

// Ensure 'Perform default action' in the actions tab invokes a click event.
AX_TEST_F('ChromeVoxPanelTest', 'PerformDoDefaultAction', async function() {
  const rootNode = await this.runWithLoadedTree(`<button>OK</button>`);
  const button = rootNode.find({role: RoleType.BUTTON});
  await this.waitForEvent(button, chrome.automation.EventType.FOCUS);
  CommandHandlerInterface.instance.onCommand('showActionsMenu');
  await this.waitForMenu('panel_menu_actions');
  this.fireMockEvent('ArrowDown');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Start Or End Selection');
  this.fireMockEvent('ArrowDown');
  this.fireMockEvent('ArrowDown');
  await this.assertActiveMenuItem(
      'panel_menu_actions', 'Perform default action');
  this.fireMockEvent('Enter');
  await this.waitForEvent(button, chrome.automation.EventType.CLICKED);
});

AX_TEST_F('ChromeVoxPanelTest', 'PanVirtualBrailleDisplay', async function() {
  await this.runWithLoadedTree(this.linksDoc);

  CommandHandlerInterface.instance.onCommand('toggleBrailleCaptions');

  // Mock out ChromeVox.braille to confirm that the commands are routed from the
  // panel context to the background context.
  let panLeft;
  let panRight;
  const panLeftDone = new Promise(resolve => panLeft = resolve);
  const panRightDone = new Promise(resolve => panRight = resolve);
  ChromeVox.braille = {panLeft, panRight};

  this.braillePanLeft();
  await panLeftDone;

  this.braillePanRight();
  await panRightDone;
});
