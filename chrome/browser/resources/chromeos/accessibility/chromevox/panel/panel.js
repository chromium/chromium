// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox panel and menus.
 */
import {constants} from '../../common/constants.js';
import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {LocalStorage} from '../../common/local_storage.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {BrailleCommandData} from '../common/braille/braille_command_data.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Command, CommandCategory, CommandStore} from '../common/command_store.js';
import {EventSourceType} from '../common/event_source_type.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {KeyMap} from '../common/key_map.js';
import {KeyUtil} from '../common/key_util.js';
import {LocaleOutputHelper} from '../common/locale_output_helper.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {ALL_PANEL_MENU_NODE_DATA, PanelNodeMenuData, PanelNodeMenuId, PanelNodeMenuItemData} from '../common/panel_menu_data.js';
import {QueueMode} from '../common/tts_types.js';

import {ISearchUI} from './i_search_ui.js';
import {PanelInterface} from './panel_interface.js';
import {PanelMenu, PanelNodeMenu, PanelSearchMenu} from './panel_menu.js';
import {PanelMode, PanelModeInfo} from './panel_mode.js';

/** Class to manage the panel. */
export class Panel extends PanelInterface {
  /** @private */
  constructor() {
    super();
    /**
     * The currently active menu, if any.
     * @private {PanelMenu}
     */
    this.activeMenu_ = null;

    /** @private {string} */
    this.lastMenu_ = '';

    /** @private {!PanelMode} */
    this.mode_ = PanelMode.COLLAPSED;

    /**
     * The array of top-level menus.
     * @private {!Array<PanelMenu>}
     */
    this.menus_ = [];

    /** @private {!Object<!PanelNodeMenuId, !PanelNodeMenu>} */
    this.nodeMenuDictionary_ = {};

    /** @private {boolean} */
    this.originalStickyState_ = false;

    /** @private {Window} */
    this.ownerWindow_ = window;

    /** @private {?(function(): !Promise)} */
    this.pendingCallback_ = null;

    /** @private {?PanelSearchMenu} */
    this.searchMenu_ = null;

    /** @private {string} */
    this.sessionState_ = '';

    /** @private {Object} */
    this.tutorial_ = null;

    /** @private {Element} */
    this.brailleContainer_ = $('braille-container');
    /** @private {Element} */
    this.brailleTableElement_ = $('braille-table');
    /** @private {Element} */
    this.brailleTableElement2_ = $('braille-table2');
    /** @private {Element} */
    this.searchContainer_ = $('search-container');
    /** @private {!Element} */
    this.searchInput_ = /** @type {!Element} */ ($('search'));
    /** @private {Element} */
    this.speechContainer_ = $('speech-container');
    /** @private {Element} */
    this.speechElement_ = $('speech');

    /** @private {boolean} */
    this.disableRestartTutorialNudgesForTesting_ = false;
    /** @private {boolean} */
    this.mockTouchGestureSourceForTesting_ = false;
    /** @private {boolean} */
    this.tutorialReadyForTesting_ = false;

    this.initListeners_();
  }

  /** @private */
  initListeners_() {
    chrome.loginState.getSessionState(state => this.updateSessionState_(state));
    chrome.loginState.onSessionStateChanged.addListener(
        state => this.updateSessionState_(state));
    $('braille-pan-left')
        .addEventListener('click', () => this.onPanLeft_(), false);
    $('braille-pan-right')
        .addEventListener('click', () => this.onPanRight_(), false);
    $('menus_button').addEventListener('mousedown', Panel.onOpenMenus, false);
    $('options').addEventListener('click', Panel.onOptions, false);
    $('close').addEventListener('click', Panel.onClose, false);

    document.addEventListener('keydown', Panel.onKeyDown, false);
    document.addEventListener('mouseup', Panel.onMouseUp, false);
    window.addEventListener(
        'storage', event => this.onStorageChanged_(event), false);
    window.addEventListener(
        'message', message => this.onMessage_(message), false);
    window.addEventListener('blur', event => this.onBlur_(event), false);
    window.addEventListener('hashchange', () => this.onHashChange_(), false);

    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ADD_MENU_ITEM,
        itemData => Panel.addNodeMenuItem(itemData));
    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ON_CURRENT_RANGE_CHANGED,
        () => Panel.onCurrentRangeChanged());
  }

  /** Initialize the panel. */
  static async init() {
    if (Panel.instance_) {
      throw new Error('Cannot call Panel.init() more than once');
    }

    await LocalStorage.init();
    LocaleOutputHelper.init();
    Panel.instance_ = new Panel();
    PanelInterface.instance = Panel.instance_;

    Panel.updateFromPrefs();
    Msgs.addTranslatedMessagesToDom(document);

    switch (location.search.slice(1)) {
      case 'tutorial':
        Panel.onTutorial();
    }
  }

  /** @override */
  setPendingCallback(callback) {
    this.pendingCallback_ = callback;
  }

  /**
   * Enables touch gesture mode for testing.
   */
  static setTouchGestureSourceForTesting() {
    Panel.instance_.mockTouchGestureSourceForTesting_ = true;
  }

  /**
   * Adds BackgroundBridge to the global object so that tests can mock it.
   */
  static exportBackgroundBridgeForTesting() {
    window.BackgroundBridge = BackgroundBridge;
  }

  /**
   * Update the display based on prefs.
   */
  static updateFromPrefs() {
    if (Panel.instance_.mode_ === PanelMode.SEARCH) {
      Panel.instance_.speechContainer_.hidden = true;
      Panel.instance_.brailleContainer_.hidden = true;
      Panel.instance_.searchContainer_.hidden = false;
      return;
    }

    Panel.instance_.speechContainer_.hidden = false;
    Panel.instance_.brailleContainer_.hidden = false;
    Panel.instance_.searchContainer_.hidden = true;

    if (LocalStorage.get('brailleCaptions')) {
      Panel.instance_.speechContainer_.style.visibility = 'hidden';
      Panel.instance_.brailleContainer_.style.visibility = 'visible';
    } else {
      Panel.instance_.speechContainer_.style.visibility = 'visible';
      Panel.instance_.brailleContainer_.style.visibility = 'hidden';
    }
  }

  /**
   * Execute a command to update the panel.
   *
   * @param {PanelCommand} command The command to execute.
   */
  static exec(command) {
    /**
     * Escape text so it can be safely added to HTML.
     * @param {*} str Text to be added to HTML, will be cast to string.
     * @return {string} The escaped string.
     */
    function escapeForHtml(str) {
      return String(str)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/\>/g, '&gt;')
          .replace(/"/g, '&quot;')
          .replace(/'/g, '&#039;')
          .replace(/\//g, '&#x2F;');
    }

    switch (command.type) {
      case PanelCommandType.CLEAR_SPEECH:
        Panel.instance_.speechElement_.innerHTML = '';
        break;
      case PanelCommandType.ADD_NORMAL_SPEECH:
        if (Panel.instance_.speechElement_.innerHTML !== '') {
          Panel.instance_.speechElement_.innerHTML += '&nbsp;&nbsp;';
        }
        Panel.instance_.speechElement_.innerHTML +=
            '<span class="usertext">' + escapeForHtml(command.data) + '</span>';
        break;
      case PanelCommandType.ADD_ANNOTATION_SPEECH:
        if (Panel.instance_.speechElement_.innerHTML !== '') {
          Panel.instance_.speechElement_.innerHTML += '&nbsp;&nbsp;';
        }
        Panel.instance_.speechElement_.innerHTML += escapeForHtml(command.data);
        break;
      case PanelCommandType.UPDATE_BRAILLE:
        Panel.onUpdateBraille(command.data);
        break;
      case PanelCommandType.OPEN_MENUS:
        Panel.onOpenMenus(undefined, command.data);
        break;
      case PanelCommandType.OPEN_MENUS_MOST_RECENT:
        Panel.onOpenMenus(undefined, Panel.instance_.lastMenu_);
        break;
      case PanelCommandType.SEARCH:
        Panel.onSearch();
        break;
      case PanelCommandType.TUTORIAL:
        Panel.onTutorial();
        break;
      case PanelCommandType.CLOSE_CHROMEVOX:
        Panel.onClose();
      case PanelCommandType.ENABLE_TEST_HOOKS:
        window.Panel = Panel;
        break;
    }
  }

  /**
   * Sets the mode, which determines the size of the panel and what objects
   *     are shown or hidden.
   * @param {PanelMode} mode The new mode.
   */
  static setMode(mode) {
    if (Panel.instance_.mode_ === mode) {
      return;
    }

    // Change the title of ChromeVox menu based on menu's state.
    $('menus_title')
        .setAttribute(
            'msgid',
            mode === PanelMode.FULLSCREEN_MENUS ? 'menus_collapse_title' :
                                                  'menus_title');
    Msgs.addTranslatedMessagesToDom(document);

    Panel.instance_.mode_ = mode;

    document.title = Msgs.getMsg(PanelModeInfo[Panel.instance_.mode_].title);

    // Fully qualify the path here because this function might be called with a
    // window object belonging to the background page.
    Panel.instance_.ownerWindow_.location =
        chrome.extension.getURL('chromevox/panel/panel.html') +
        PanelModeInfo[Panel.instance_.mode_].location;

    $('main').hidden =
        (Panel.instance_.mode_ === PanelMode.FULLSCREEN_TUTORIAL);
    $('menus_background').hidden =
        (Panel.instance_.mode_ !== PanelMode.FULLSCREEN_MENUS);
    // Interactive tutorial elements may not have been loaded yet.
    const iTutorialContainer = $('chromevox-tutorial-container');
    if (iTutorialContainer) {
      iTutorialContainer.hidden =
          (Panel.instance_.mode_ !== PanelMode.FULLSCREEN_TUTORIAL);
    }

    Panel.updateFromPrefs();

    // Change the orientation of the triangle next to the menus button to
    // indicate whether the menu is open or closed.
    if (mode === PanelMode.FULLSCREEN_MENUS) {
      $('triangle').style.transform = 'rotate(180deg)';
    } else if (mode === PanelMode.COLLAPSED) {
      $('triangle').style.transform = '';
    }
  }

  /**
   * Open / show the ChromeVox Menus.
   * @param {Event=} opt_event An optional event that triggered this.
   * @param {*=} opt_activateMenuTitle Title msg id of menu to open.
   */
  static async onOpenMenus(opt_event, opt_activateMenuTitle) {
    // If the menu was already open, close it now and exit early.
    if (Panel.instance_.mode_ !== PanelMode.COLLAPSED) {
      Panel.setMode(PanelMode.COLLAPSED);
      return;
    }

    // Eat the event so that a mousedown isn't turned into a drag, allowing
    // users to click-drag-release to select a menu item.
    if (opt_event) {
      opt_event.stopPropagation();
      opt_event.preventDefault();
    }

    await BackgroundBridge.PanelBackground.saveCurrentNode();
    Panel.setMode(PanelMode.FULLSCREEN_MENUS);

    const onFocusDo = async () => {
      window.removeEventListener('focus', onFocusDo);
      // Clear any existing menus and clear the callback.
      Panel.clearMenus();
      Panel.pendingCallback_ = null;

      const eventSourceState = await BackgroundBridge.EventSourceState.get();
      const touchScreen =
          (eventSourceState === EventSourceType.TOUCH_GESTURE ||
           this.instance_.mockTouchGestureSourceForTesting_);

      // Build the top-level menus.
      const searchMenu = Panel.addSearchMenu('panel_search_menu');
      const jumpMenu = Panel.addMenu('panel_menu_jump');
      const speechMenu = Panel.addMenu('panel_menu_speech');
      const touchMenu =
          touchScreen ? Panel.addMenu('panel_menu_touchgestures') : null;
      const tabsMenu = Panel.addMenu('panel_menu_tabs');
      const chromevoxMenu = Panel.addMenu('panel_menu_chromevox');
      const actionsMenu = Panel.addMenu('panel_menu_actions');

      // Add a menu item that opens the full list of ChromeBook keyboard
      // shortcuts. We want this to be at the top of the ChromeVox menu.
      let localizedSlash = await new Promise(
          resolve =>
              chrome.accessibilityPrivate.getLocalizedDomKeyStringForKeyCode(
                  KeyCode.OEM_2, resolve));
      if (!localizedSlash) {
        localizedSlash = '/';
      }
      chromevoxMenu.addMenuItem(
          Msgs.getMsg('open_keyboard_shortcuts_menu'),
          `Ctrl+Alt+${localizedSlash}`, '', '', async function() {
            EventGenerator.sendKeyPress(
                KeyCode.OEM_2 /* forward slash */, {'ctrl': true, 'alt': true});
          });

      // Create a mapping between categories from CommandStore, and our
      // top-level menus. Some categories aren't mapped to any menu.
      const categoryToMenu = {
        [CommandCategory.NAVIGATION]: jumpMenu,
        [CommandCategory.JUMP_COMMANDS]: jumpMenu,
        [CommandCategory.OVERVIEW]: jumpMenu,
        [CommandCategory.TABLES]: jumpMenu,
        [CommandCategory.CONTROLLING_SPEECH]: speechMenu,
        [CommandCategory.INFORMATION]: speechMenu,
        [CommandCategory.MODIFIER_KEYS]: chromevoxMenu,
        [CommandCategory.HELP_COMMANDS]: chromevoxMenu,
        [CommandCategory.ACTIONS]: actionsMenu,

        [CommandCategory.BRAILLE]: null,
        [CommandCategory.DEVELOPER]: null,
      };

      // TODO(accessibility): Commands should be based off of CommandStore and
      // not the keymap. There are commands that don't have a key binding (e.g.
      // commands for touch).

      // Get the key map.
      const keymap = KeyMap.get();

      // Make a copy of the key bindings, get the localized title of each
      // command, and then sort them.
      const sortedBindings = keymap.bindings().slice();
      for (let binding, i = 0; binding = sortedBindings[i]; i++) {
        const command = binding.command;
        const keySeq = binding.sequence;
        binding.keySeq = await KeyUtil.keySequenceToString(keySeq, true);
        const titleMsgId = CommandStore.messageForCommand(command);
        if (!titleMsgId) {
          console.error('No localization for: ' + command);
          binding.title = '';
          continue;
        }
        let title = Msgs.getMsg(titleMsgId);
        // Convert to title case.
        title = title.replace(/\w\S*/g, function(word) {
          return word.charAt(0).toUpperCase() + word.substr(1);
        });
        binding.title = title;
      }
      sortedBindings.sort(function(binding1, binding2) {
        return binding1.title.localeCompare(binding2.title);
      });

      // Insert items from the bindings into the menus.
      const sawBindingSet = {};
      const bindingMap = new Map();
      const gestures = Object.keys(GestureCommandData.GESTURE_COMMAND_MAP);
      sortedBindings.forEach(binding => {
        const command = binding.command;
        bindingMap.set(binding.command, binding);
        if (sawBindingSet[command]) {
          return;
        }
        sawBindingSet[command] = true;
        const category = CommandStore.categoryForCommand(binding.command);
        const menu = category ? categoryToMenu[category] : null;
        if (binding.title && menu) {
          let keyText;
          let brailleText;
          let gestureText;
          if (touchScreen) {
            for (let i = 0, gesture; gesture = gestures[i]; i++) {
              const data = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
              if (data && data.command === command) {
                gestureText = Msgs.getMsg(data.msgId);
                break;
              }
            }
          } else {
            keyText = binding.keySeq;
            brailleText =
                BrailleCommandData.getDotShortcut(binding.command, true);
          }

          menu.addMenuItem(
              binding.title, keyText, brailleText, gestureText,
              () => BackgroundBridge.CommandHandler.onCommand(binding.command),
              binding.command);
        }
      });

      // Add Touch Gestures menu items.
      if (touchScreen) {
        const touchGestureItems = [];
        for (const key in GestureCommandData.GESTURE_COMMAND_MAP) {
          const command =
              GestureCommandData.GESTURE_COMMAND_MAP[key]['command'];
          if (!command) {
            continue;
          }

          const gestureText =
              Msgs.getMsg(GestureCommandData.GESTURE_COMMAND_MAP[key]['msgId']);
          const msgForCmd =
              GestureCommandData
                  .GESTURE_COMMAND_MAP[key]['commandDescriptionMsgId'] ||
              CommandStore.messageForCommand(command);
          const titleText = Msgs.getMsg(msgForCmd);
          touchGestureItems.push({titleText, gestureText, command});
        }

        touchGestureItems.sort(function(item1, item2) {
          return item1.titleText.localeCompare(item2.titleText);
        });

        for (const item of touchGestureItems) {
          touchMenu.addMenuItem(
              item.titleText, '', '', item.gestureText,
              () => BackgroundBridge.CommandHandler.onCommand(item.command),
              item.command);
        }
      }

      // Add all open tabs to the Tabs menu.
      const data = await BackgroundBridge.PanelBackground.getTabMenuData();
      for (const menuInfo of data) {
        tabsMenu.addMenuItem(menuInfo.title, '', '', '', async function() {
          BackgroundBridge.PanelBackground.focusTab(
              menuInfo.windowId, menuInfo.tabId);
        });
      }

      if (Panel.instance_.sessionState_ !== 'IN_SESSION') {
        tabsMenu.disable();
        // Disable commands that contain the property 'denyOOBE'.
        for (let i = 0; i < Panel.instance_.menus_.length; ++i) {
          const menu = Panel.instance_.menus_[i];
          for (let j = 0; j < menu.items.length; ++j) {
            const item = menu.items[j];
            if (CommandStore.denySignedOut(
                    /** @type {!Command} */ (item.element.id))) {
              item.disable();
            }
          }
        }
      }

      // Add a menu item that disables / closes ChromeVox.
      chromevoxMenu.addMenuItem(
          Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', '', '',
          async function() {
            Panel.onClose();
          });

      for (const menuData of ALL_PANEL_MENU_NODE_DATA) {
        Panel.addNodeMenu(menuData);
      }
      await BackgroundBridge.PanelBackground.createAllNodeMenuBackgrounds(
          opt_activateMenuTitle);

      const actions =
          await BackgroundBridge.PanelBackground.getActionsForCurrentNode();
      for (const standardAction of actions.standardActions) {
        const actionMsg = Panel.ACTION_TO_MSG_ID[standardAction];
        if (!actionMsg) {
          continue;
        }
        const commandName = CommandStore.commandForMessage(actionMsg);
        const command = bindingMap.get(commandName);
        const shortcutName = command ? command.keySeq : '';
        const actionDesc = Msgs.getMsg(actionMsg);
        actionsMenu.addMenuItem(
            actionDesc, shortcutName, '' /* menuItemBraille */,
            '' /* gesture */,
            () => BackgroundBridge.PanelBackground
                      .performStandardActionOnCurrentNode(standardAction));
      }

      for (const customAction of actions.customActions) {
        actionsMenu.addMenuItem(
            customAction.description, '' /* menuItemShortcut */,
            '' /* menuItemBraille */, '' /* gesture */,
            () => BackgroundBridge.PanelBackground
                      .performCustomActionOnCurrentNode(customAction.id));
      }

      // Activate either the specified menu or the search menu.
      // Search menu can be null, since it is hidden behind a flag.
      let selectedMenu =
          Panel.instance_.searchMenu_ || Panel.instance_.menus_[0];
      for (let i = 0; i < Panel.instance_.menus_.length; i++) {
        if (Panel.instance_.menus_[i].menuMsg === opt_activateMenuTitle) {
          selectedMenu = Panel.instance_.menus_[i];
        }
      }

      const activateFirstItem = (selectedMenu !== Panel.instance_.searchMenu_);
      Panel.activateMenu(selectedMenu, activateFirstItem);
    };

    // The panel does not get focus immediately when we request to be full
    // screen (handled in ChromeVoxPanel natively on hash changed). Wait, if
    // needed, for focus to begin initialization.
    if (document.hasFocus()) {
      onFocusDo();
    } else {
      window.addEventListener('focus', onFocusDo);
    }
  }

  /** Open incremental search. */
  static async onSearch() {
    Panel.setMode(PanelMode.SEARCH);
    Panel.clearMenus();
    Panel.pendingCallback_ = null;
    Panel.updateFromPrefs();
    await ISearchUI.init(Panel.instance_.searchInput_);
  }

  /**
   * Clear any previous menus. The menus are all regenerated each time the
   * menus are opened.
   */
  static clearMenus() {
    while (Panel.instance_.menus_.length) {
      const menu = Panel.instance_.menus_.pop();
      $('menu-bar').removeChild(menu.menuBarItemElement);
      $('menus_background').removeChild(menu.menuContainerElement);
    }
    if (Panel.instance_.activeMenu_) {
      Panel.instance_.lastMenu_ = Panel.instance_.activeMenu_.menuMsg;
    }
    Panel.instance_.activeMenu_ = null;
  }

  /**
   * Create a new menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  static addMenu(menuMsg) {
    const menu = new PanelMenu(menuMsg);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener('mouseover', function() {
      Panel.activateMenu(menu, true /* activateFirstItem */);
    }, false);
    menu.menuBarItemElement.addEventListener(
        'mouseup', Panel.onMouseUpOnMenuTitle_.bind(this, menu), false);
    $('menus_background').appendChild(menu.menuContainerElement);
    Panel.instance_.menus_.push(menu);
    return menu;
  }

  /**
   * Updates the content shown on the virtual braille display.
   * @param {*=} data The data sent through the PanelCommand.
   */
  static onUpdateBraille(data) {
    const groups = data.groups;
    const cols = data.cols;
    const rows = data.rows;
    const sideBySide = LocalStorage.get('brailleSideBySide');

    const addBorders = function(event) {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        cell.className = 'highlighted-cell';
        const companionIDs = cell.getAttribute('data-companionIDs');
        companionIDs.split(' ').forEach(
            companionID => $(companionID).className = 'highlighted-cell');
      }
    };

    const removeBorders = function(event) {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        cell.className = 'unhighlighted-cell';
        const companionIDs = cell.getAttribute('data-companionIDs');
        companionIDs.split(' ').forEach(
            companionID => $(companionID).className = 'unhighlighted-cell');
      }
    };

    const routeCursor = function(event) {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        const displayPosition = parseInt(cell.id.split('-')[0], 10);
        if (Number.isNaN(displayPosition)) {
          throw new Error(
              'The display position is calculated assuming that the cell ID ' +
              'is formatted like int-string. For example, 0-brailleCell is a ' +
              'valid cell ID.');
        }
        chrome.extension.getBackgroundPage()['ChromeVox'].braille.route(
            displayPosition);
      }
    };

    Panel.instance_.brailleContainer_.addEventListener('mouseover', addBorders);
    Panel.instance_.brailleContainer_.addEventListener(
        'mouseout', removeBorders);
    Panel.instance_.brailleContainer_.addEventListener('click', routeCursor);

    // Clear the tables.
    let rowCount = Panel.instance_.brailleTableElement_.rows.length;
    for (let i = 0; i < rowCount; i++) {
      Panel.instance_.brailleTableElement_.deleteRow(0);
    }
    rowCount = Panel.instance_.brailleTableElement2_.rows.length;
    for (let i = 0; i < rowCount; i++) {
      Panel.instance_.brailleTableElement2_.deleteRow(0);
    }

    let row1;
    let row2;
    // Number of rows already written.
    rowCount = 0;
    // Number of cells already written in this row.
    let cellCount = cols;
    for (let i = 0; i < groups.length; i++) {
      if (cellCount === cols) {
        cellCount = 0;
        // Check if we reached the limit on the number of rows we can have.
        if (rowCount === rows) {
          break;
        }
        rowCount++;
        row1 = Panel.instance_.brailleTableElement_.insertRow(-1);
        if (sideBySide) {
          // Side by side.
          row2 = Panel.instance_.brailleTableElement2_.insertRow(-1);
        } else {
          // Interleaved.
          row2 = Panel.instance_.brailleTableElement_.insertRow(-1);
        }
      }

      const topCell = row1.insertCell(-1);
      topCell.innerHTML = groups[i][0];
      topCell.id = i + '-textCell';
      topCell.setAttribute('data-companionIDs', i + '-brailleCell');
      topCell.className = 'unhighlighted-cell';

      let bottomCell = row2.insertCell(-1);
      bottomCell.id = i + '-brailleCell';
      bottomCell.setAttribute('data-companionIDs', i + '-textCell');
      bottomCell.className = 'unhighlighted-cell';
      if (cellCount + groups[i][1].length > cols) {
        let brailleText = groups[i][1];
        while (cellCount + brailleText.length > cols) {
          // At this point we already have a bottomCell to fill, so fill it.
          bottomCell.innerHTML = brailleText.substring(0, cols - cellCount);
          // Update to see what we still have to fill.
          brailleText = brailleText.substring(cols - cellCount);
          // Make new row.
          if (rowCount === rows) {
            break;
          }
          rowCount++;
          row1 = Panel.instance_.brailleTableElement_.insertRow(-1);
          if (sideBySide) {
            // Side by side.
            row2 = Panel.instance_.brailleTableElement2_.insertRow(-1);
          } else {
            // Interleaved.
            row2 = Panel.instance_.brailleTableElement_.insertRow(-1);
          }
          const bottomCell2 = row2.insertCell(-1);
          bottomCell2.id = i + '-brailleCell2';
          bottomCell2.setAttribute(
              'data-companionIDs', i + '-textCell ' + i + '-brailleCell');
          bottomCell.setAttribute(
              'data-companionIDs',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');
          topCell.setAttribute(
              'data-companionID2',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');

          bottomCell2.className = 'unhighlighted-cell';
          bottomCell = bottomCell2;
          cellCount = 0;
        }
        // Fill the rest.
        bottomCell.innerHTML = brailleText;
        cellCount = brailleText.length;
      } else {
        bottomCell.innerHTML = groups[i][1];
        cellCount += groups[i][1].length;
      }
    }
  }

  /**
   * Create a new node menu with the given name and add it to the menu bar.
   * @param {!PanelNodeMenuData} menuData The title/predicate for the new menu.
   */
  static addNodeMenu(menuData) {
    const menu = new PanelNodeMenu(menuData.titleId);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener('mouseover', () => {
      Panel.activateMenu(menu, true /* activateFirstItem */);
    });
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => Panel.onMouseUpOnMenuTitle_(menu, event));
    $('menus_background').appendChild(menu.menuContainerElement);
    Panel.instance_.menus_.push(menu);
    Panel.instance_.nodeMenuDictionary_[menuData.menuId] = menu;
  }

  /** @param {!PanelNodeMenuItemData} itemData */
  static addNodeMenuItem(itemData) {
    Panel.instance_.nodeMenuDictionary_[itemData.menuId].addItemFromData(
        itemData);
  }

  /**
   * Create a new search menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  static addSearchMenu(menuMsg) {
    Panel.instance_.searchMenu_ = new PanelSearchMenu(menuMsg);
    // Add event listerns to search bar.
    Panel.instance_.searchMenu_.searchBar.addEventListener(
        'input', Panel.onSearchBarQuery, false);
    Panel.instance_.searchMenu_.searchBar.addEventListener(
        'mouseup', function(event) {
          // Clicking in the panel causes us to either activate an item or close
          // the menus altogether. Prevent that from happening if we click the
          // search bar.
          event.preventDefault();
          event.stopPropagation();
        }, false);

    $('menu-bar').appendChild(Panel.instance_.searchMenu_.menuBarItemElement);
    Panel.instance_.searchMenu_.menuBarItemElement.addEventListener(
        'mouseover', function(event) {
          Panel.activateMenu(
              Panel.instance_.searchMenu_, false /* activateFirstItem */);
        }, false);
    Panel.instance_.searchMenu_.menuBarItemElement.addEventListener(
        'mouseup',
        Panel.onMouseUpOnMenuTitle_.bind(this, Panel.instance_.searchMenu_),
        false);
    $('menus_background')
        .appendChild(Panel.instance_.searchMenu_.menuContainerElement);
    Panel.instance_.menus_.push(Panel.instance_.searchMenu_);
    return Panel.instance_.searchMenu_;
  }

  /**
   * Activate a menu, which implies hiding the previous active menu.
   * @param {PanelMenu} menu The new menu to activate.
   * @param {boolean} activateFirstItem Whether or not we should activate the
   *     menu's
   * first item.
   */
  static activateMenu(menu, activateFirstItem) {
    if (menu === Panel.instance_.activeMenu_) {
      return;
    }

    if (Panel.instance_.activeMenu_) {
      Panel.instance_.activeMenu_.deactivate();
      Panel.instance_.activeMenu_ = null;
    }

    Panel.instance_.activeMenu_ = menu;
    Panel.pendingCallback_ = null;

    if (Panel.instance_.activeMenu_) {
      Panel.instance_.activeMenu_.activate(activateFirstItem);
    }
  }

  /**
   * Sets the index of the current active menu to be 0.
   */
  static scrollToTop() {
    Panel.instance_.activeMenu_.scrollToTop();
  }

  /**
   * Sets the index of the current active menu to be the last index.
   */
  static scrollToBottom() {
    Panel.instance_.activeMenu_.scrollToBottom();
  }

  /**
   * Advance the index of the current active menu by |delta|.
   * @param {number} delta The number to add to the active menu index.
   */
  static advanceActiveMenuBy(delta) {
    let activeIndex = -1;
    for (let i = 0; i < Panel.instance_.menus_.length; i++) {
      if (Panel.instance_.activeMenu_ === Panel.instance_.menus_[i]) {
        activeIndex = i;
        break;
      }
    }

    if (activeIndex >= 0) {
      activeIndex += delta;
      activeIndex = (activeIndex + Panel.instance_.menus_.length) %
          Panel.instance_.menus_.length;
    } else {
      if (delta >= 0) {
        activeIndex = 0;
      } else {
        activeIndex = Panel.instance_.menus_.length - 1;
      }
    }

    activeIndex = Panel.findEnabledMenuIndex_(activeIndex, delta > 0 ? 1 : -1);
    if (activeIndex === -1) {
      return;
    }

    Panel.activateMenu(
        Panel.instance_.menus_[activeIndex], true /* activateFirstItem */);
  }

  /**
   * Starting at |startIndex|, looks for an enabled menu.
   * @param {number} startIndex
   * @param {number} delta
   * @return {number} The index of the enabled menu. -1 if not found.
   */
  static findEnabledMenuIndex_(startIndex, delta) {
    const endIndex = (delta > 0) ? Panel.instance_.menus_.length : -1;
    while (startIndex !== endIndex) {
      if (Panel.instance_.menus_[startIndex].enabled) {
        return startIndex;
      }
      startIndex += delta;
    }
    return -1;
  }

  /**
   * Advance the index of the current active menu item by |delta|.
   * @param {number} delta The number to add to the active menu item index.
   */
  static advanceItemBy(delta) {
    if (Panel.instance_.activeMenu_) {
      Panel.instance_.activeMenu_.advanceItemBy(delta);
    }
  }

  /**
   * Called when the user releases the mouse button. If it's anywhere other
   * than on the menus button, close the menus and return focus to the page,
   * and if the mouse was released over a menu item, execute that item's
   * callback.
   * @param {Event} event The mouse event.
   */
  static onMouseUp(event) {
    if (!Panel.instance_.activeMenu_) {
      return;
    }

    let target = event.target;
    while (target && !target.classList.contains('menu-item')) {
      // Allow the user to click and release on the menu button and leave
      // the menu button.
      if (target.id === 'menus_button') {
        return;
      }

      target = target.parentElement;
    }

    if (target && Panel.instance_.activeMenu_) {
      Panel.pendingCallback_ =
          Panel.instance_.activeMenu_.getCallbackForElement(target);
    }
    PanelInterface.instance.closeMenusAndRestoreFocus();
  }

  /**
   * Activate a menu whose title has been clicked. Stop event propagation at
   * this point so we don't close the ChromeVox menus and restore focus.
   * @param {PanelMenu} menu The menu we would like to activate.
   * @param {Event} mouseUpEvent The mouseup event.
   * @private
   */
  static onMouseUpOnMenuTitle_(menu, mouseUpEvent) {
    Panel.activateMenu(menu, true /* activateFirstItem */);
    mouseUpEvent.preventDefault();
    mouseUpEvent.stopPropagation();
  }

  /**
   * Called when a key is pressed. Handle arrow keys to navigate the menus,
   * Esc to close, and Enter/Space to activate an item.
   * @param {Event} event The key event.
   */
  static onKeyDown(event) {
    if (event.key === 'Escape' &&
        Panel.instance_.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      Panel.setMode(PanelMode.COLLAPSED);
      return;
    }

    if (!Panel.instance_.activeMenu_) {
      return;
    }

    if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
      return;
    }

    // We need special logic for navigating the search bar.
    // If left/right arrow are pressed, we should adjust the search bar's
    // cursor. We only want to advance the active menu if we are at the
    // beginning/end of the search bar's contents.
    if (Panel.instance_.searchMenu_ &&
        event.target === Panel.instance_.searchMenu_.searchBar) {
      switch (event.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          if (event.target.value) {
            const cursorIndex = event.target.selectionStart +
                (event.key === 'ArrowRight' ? 1 : -1);
            const queryLength = event.target.value.length;
            if (cursorIndex >= 0 && cursorIndex <= queryLength) {
              return;
            }
          }
          break;
        case ' ':
          return;
      }
    }

    switch (event.key) {
      case 'ArrowLeft':
        Panel.advanceActiveMenuBy(-1);
        break;
      case 'ArrowRight':
        Panel.advanceActiveMenuBy(1);
        break;
      case 'ArrowUp':
        Panel.advanceItemBy(-1);
        break;
      case 'ArrowDown':
        Panel.advanceItemBy(1);
        break;
      case 'Escape':
        PanelInterface.instance.closeMenusAndRestoreFocus();
        break;
      case 'PageUp':
        Panel.advanceItemBy(10);
        break;
      case 'PageDown':
        Panel.advanceItemBy(-10);
        break;
      case 'Home':
        Panel.scrollToTop();
        break;
      case 'End':
        Panel.scrollToBottom();
        break;
      case 'Enter':
      case ' ':
        Panel.pendingCallback_ = Panel.getCallbackForCurrentItem();
        PanelInterface.instance.closeMenusAndRestoreFocus();
        break;
      default:
        // Don't mark this event as handled.
        return;
    }

    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Open the ChromeVox Options.
   */
  static onOptions() {
    chrome.runtime.openOptionsPage();
    Panel.setMode(PanelMode.COLLAPSED);
  }

  /**
   * Exit ChromeVox.
   */
  static onClose() {
    // Change the url fragment to 'close', which signals the native code
    // to exit ChromeVox.
    Panel.instance_.ownerWindow_.location =
        chrome.extension.getURL('chromevox/panel/panel.html') + '#close';
  }

  /**
   * Get the callback for whatever item is currently selected.
   * @return {Function} The callback for the current item.
   */
  static getCallbackForCurrentItem() {
    if (Panel.instance_.activeMenu_) {
      return Panel.instance_.activeMenu_.getCallbackForCurrentItem();
    }
    return null;
  }

  /** @override */
  async closeMenusAndRestoreFocus() {
    const pendingCallback = Panel.pendingCallback_;
    Panel.pendingCallback_ = null;

    // Prepare the watcher before close the panel so that the watcher won't miss
    // panel collapse signal.
    await BackgroundBridge.PanelBackground.setPanelCollapseWatcher;

    // Make sure all menus are cleared to avoid bogus output when we re-open.
    Panel.clearMenus();

    // Make sure we're not in full-screen mode.
    Panel.setMode(PanelMode.COLLAPSED);

    Panel.instance_.activeMenu_ = null;

    await BackgroundBridge.PanelBackground.waitForPanelCollapse();

    if (pendingCallback) {
      await pendingCallback();
    }
    BackgroundBridge.PanelBackground.clearSavedNode();
  }

  /** Open the tutorial. */
  static onTutorial() {
    chrome.chromeosInfoPrivate.isTabletModeEnabled(enabled => {
      // Use tablet mode to decide the medium for the tutorial.
      const medium = enabled ? constants.InteractionMedium.TOUCH :
                               constants.InteractionMedium.KEYBOARD;
      if (!$('chromevox-tutorial')) {
        let curriculum = null;
        if (Panel.instance_.sessionState_ ===
            chrome.loginState.SessionState.IN_OOBE_SCREEN) {
          // We currently support two mediums: keyboard and touch, which is why
          // we can decide the curriculum using a ternary statement.
          curriculum = medium === constants.InteractionMedium.KEYBOARD ?
              'quick_orientation' :
              'touch_orientation';
        }
        Panel.createITutorial(curriculum, medium);
      }

      Panel.setMode(PanelMode.FULLSCREEN_TUTORIAL);
      if (Panel.instance_.tutorial_ && Panel.instance_.tutorial_.show) {
        Panel.instance_.tutorial_.medium = medium;
        Panel.instance_.tutorial_.show();
      }
    });
  }

  /**
   * Creates a <chromevox-tutorial> element and adds it to the dom.
   * @param {(string|null)} curriculum
   * @param {constants.InteractionMedium} medium
   */
  static createITutorial(curriculum, medium) {
    const tutorialScript = document.createElement('script');
    tutorialScript.src =
        '../../common/tutorial/components/chromevox_tutorial.js';
    tutorialScript.setAttribute('type', 'module');
    document.body.appendChild(tutorialScript);

    // Create tutorial container and element.
    const tutorialContainer = document.createElement('div');
    tutorialContainer.setAttribute('id', 'chromevox-tutorial-container');
    tutorialContainer.hidden = true;
    const tutorialElement = document.createElement('chromevox-tutorial');
    tutorialElement.setAttribute('id', 'chromevox-tutorial');
    if (curriculum) {
      tutorialElement.curriculum = curriculum;
    }
    tutorialElement.medium = medium;
    tutorialContainer.appendChild(tutorialElement);
    document.body.appendChild(tutorialContainer);
    Panel.instance_.tutorial_ = tutorialElement;

    // Add listeners. These are custom events fired from custom components.
    const backgroundPage = chrome.extension.getBackgroundPage();

    $('chromevox-tutorial').addEventListener('closetutorial', async evt => {
      // Ensure UserActionMonitor is destroyed before closing tutorial.
      await BackgroundBridge.UserActionMonitor.destroy();
      Panel.onCloseTutorial();
    });
    $('chromevox-tutorial').addEventListener('requestspeech', evt => {
      /**
       * @type {{
       * text: string,
       * queueMode: QueueMode,
       * properties: ({doNotInterrupt: boolean}|undefined)}}
       */
      const detail = evt.detail;
      const text = detail.text;
      const queueMode = detail.queueMode;
      const properties = detail.properties || {};
      if (!text || queueMode === undefined) {
        throw new Error(
            `Must specify text and queueMode when requesting speech from the
                tutorial`);
      }
      const cvox = backgroundPage['ChromeVox'];
      cvox.tts.speak(text, queueMode, properties);
    });
    $('chromevox-tutorial')
        .addEventListener('startinteractivemode', async evt => {
          const actions = evt.detail.actions;
          await BackgroundBridge.UserActionMonitor.create(actions);
          await BackgroundBridge.UserActionMonitor.destroy();
          if (Panel.instance_.tutorial_ &&
              Panel.instance_.tutorial_.showNextLesson) {
            Panel.instance_.tutorial_.showNextLesson();
          }
        });
    $('chromevox-tutorial')
        .addEventListener('stopinteractivemode', async evt => {
          await BackgroundBridge.UserActionMonitor.destroy();
        });
    $('chromevox-tutorial').addEventListener('requestfullydescribe', evt => {
      BackgroundBridge.CommandHandler.onCommand(Command.FULLY_DESCRIBE);
    });
    $('chromevox-tutorial').addEventListener('requestearcon', evt => {
      evt = /** @type {{detail: {earconId: string}}} */ (evt);
      const earconId = evt.detail.earconId;
      backgroundPage['ChromeVox']['earcons']['playEarcon'](earconId);
    });
    $('chromevox-tutorial').addEventListener('cancelearcon', evt => {
      evt = /** @type {{detail: {earconId: string}}} */ (evt);
      const earconId = evt.detail.earconId;
      backgroundPage['ChromeVox']['earcons']['cancelEarcon'](earconId);
    });
    $('chromevox-tutorial').addEventListener('readyfortesting', () => {
      Panel.instance_.tutorialReadyForTesting_ = true;
    });
    $('chromevox-tutorial').addEventListener('openUrl', async evt => {
      const url = evt.detail.url;
      // Ensure UserActionMonitor is destroyed before closing tutorial.
      await BackgroundBridge.UserActionMonitor.destroy();
      Panel.onCloseTutorial();
      chrome.tabs.create({url});
    });
  }

  /** Close the tutorial. */
  static onCloseTutorial() {
    Panel.setMode(PanelMode.COLLAPSED);
  }

  /**
   * Listens to changes in the menu search bar. Populates the search menu
   * with items that match the search bar's contents.
   * Note: we ignore PanelNodeMenu items and items without shortcuts.
   * @param {Event} event The input event.
   */
  static onSearchBarQuery(event) {
    if (!Panel.instance_.searchMenu_) {
      throw Error('Panel.instance_.searchMenu_ must be defined');
    }
    const query = event.target.value.toLowerCase();
    Panel.instance_.searchMenu_.clear();
    // Show the search results menu.
    Panel.activateMenu(
        Panel.instance_.searchMenu_, false /* activateFirstItem */);
    // Populate.
    if (query) {
      for (let i = 0; i < Panel.instance_.menus_.length; ++i) {
        const menu = Panel.instance_.menus_[i];
        if (menu === Panel.instance_.searchMenu_ ||
            menu instanceof PanelNodeMenu) {
          continue;
        }
        const items = menu.items;
        for (let j = 0; j < items.length; ++j) {
          const item = items[j];
          if (!item.menuItemShortcut) {
            // Only add menu items that have shortcuts.
            continue;
          }
          const itemText = item.text.toLowerCase();
          const match = itemText.includes(query) &&
              (itemText !==
               Msgs.getMsg('panel_menu_item_none').toLowerCase()) &&
              item.enabled;
          if (match) {
            Panel.instance_.searchMenu_.copyAndAddMenuItem(item);
          }
        }
      }
    }

    if (Panel.instance_.searchMenu_.items.length === 0) {
      Panel.instance_.searchMenu_.addMenuItem(
          Msgs.getMsg('panel_menu_item_none'), '', '', '', function() {});
    }
    Panel.instance_.searchMenu_.activateItem(0);
  }

  static onCurrentRangeChanged() {
    if (Panel.instance_.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      if (Panel.instance_.tutorial_ &&
          Panel.instance_.tutorial_.restartNudges &&
          !Panel.instance_.disableRestartTutorialNudgesForTesting_) {
        Panel.instance_.tutorial_.restartNudges();
      }
    }
  }

  /** @private */
  onBlur_(event) {
    if (event.target !== window || document.activeElement === document.body) {
      return;
    }

    this.closeMenusAndRestoreFocus();
  }

  /** @private */
  async onHashChange_() {
    // Save the sticky state when a user first focuses the panel.
    if (location.hash === '#fullscreen' || location.hash === '#focus') {
      this.originalStickyState_ =
          await BackgroundBridge.ChromeVoxPrefs.getStickyPref();
    }

    // If the original sticky state was on when we first entered the panel,
    // toggle it in in every case. (fullscreen/focus turns the state off,
    // collapse turns it back on).
    if (this.originalStickyState_) {
      BackgroundBridge.CommandHandler.onCommand(Command.TOGGLE_STICKY_MODE);
    }
  }

  /** @private */
  onMessage_(message) {
    const command = JSON.parse(message.data);
    Panel.exec(/** @type {PanelCommand} */ (command));
  }

  /** @private */
  onPanLeft_() {
    chrome.extension.getBackgroundPage()['ChromeVox'].braille.panLeft();
  }

  /** @private */
  onPanRight_() {
    chrome.extension.getBackgroundPage()['ChromeVox'].braille.panRight();
  }

  /** @private */
  onStorageChanged_(event) {
    if (event.key === 'brailleCaptions') {
      Panel.updateFromPrefs();
    }
  }

  /** @private */
  updateSessionState_(sessionState) {
    this.sessionState_ = sessionState;
    $('options').disabled = sessionState !== 'IN_SESSION';
  }
}

Panel.ACTION_TO_MSG_ID = {
  decrement: 'action_decrement_description',
  doDefault: 'perform_default_action',
  increment: 'action_increment_description',
  scrollBackward: 'action_scroll_backward_description',
  scrollForward: 'action_scroll_forward_description',
  showContextMenu: 'show_context_menu',
  longClick: 'force_long_click_on_current_item',
};

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {Element} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

window.addEventListener('load', async () => await Panel.init(), false);

/** @private {Panel} */
Panel.instance_;
