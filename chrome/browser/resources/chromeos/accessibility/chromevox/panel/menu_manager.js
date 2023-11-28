// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {AsyncUtil} from '../../common/async_util.js';
import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {StringUtil} from '../../common/string_util.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {BrailleCommandData} from '../common/braille/braille_command_data.js';
import {Command, CommandCategory} from '../common/command.js';
import {CommandStore} from '../common/command_store.js';
import {EventSourceType} from '../common/event_source_type.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {KeyMap} from '../common/key_map.js';
import {KeyBinding, KeySequence} from '../common/key_sequence.js';
import {KeyUtil} from '../common/key_util.js';
import {Msgs} from '../common/msgs.js';
import {ALL_PANEL_MENU_NODE_DATA, PanelNodeMenuData, PanelNodeMenuId, PanelNodeMenuItemData} from '../common/panel_menu_data.js';

import {PanelInterface} from './panel_interface.js';
import {PanelMenu, PanelNodeMenu, PanelSearchMenu} from './panel_menu.js';
import {PanelMode} from './panel_mode.js';

const $ = (id) => document.getElementById(id);

export class MenuManager {
  constructor() {
    /**
     * The currently active menu, if any.
     * @private {?PanelMenu}
     */
    this.activeMenu_ = null;

    /** @private {string} */
    this.lastMenu_ = '';

    /**
     * The array of top-level menus.
     * @private {!Array<!PanelMenu>}
     */
    this.menus_ = [];

    /** @private {!Object<!PanelNodeMenuId, !PanelNodeMenu>} */
    this.nodeMenuDictionary_ = {};

    /** @private {?PanelSearchMenu} */
    this.searchMenu_ = null;
  }

  /**
   * Activate a menu, which implies hiding the previous active menu.
   * @param {?PanelMenu} menu The new menu to activate.
   * @param {boolean} activateFirstItem Whether or not we should activate the
   *     menu's first item.
   */
  activateMenu(menu, activateFirstItem) {
    if (menu === this.activeMenu_) {
      return;
    }

    if (this.activeMenu_) {
      this.activeMenu_.deactivate();
      this.activeMenu_ = null;
    }

    this.activeMenu_ = menu;
    PanelInterface.instance.setPendingCallback(null);

    if (this.activeMenu_) {
      this.activeMenu_.activate(activateFirstItem);
    }
  }

  /**
   * @param {!PanelMenu} actionsMenu
   * @param {!Map<!Command, !KeyBinding>} bindingMap
   */
  async addActionsMenuItems(actionsMenu, bindingMap) {
    const actions =
        await BackgroundBridge.PanelBackground.getActionsForCurrentNode();
    for (const standardAction of actions.standardActions) {
      const actionMsg = ACTION_TO_MSG_ID[standardAction];
      if (!actionMsg) {
        continue;
      }
      const commandName = CommandStore.commandForMessage(actionMsg);
      let shortcutName = '';
      if (commandName) {
        const commandBinding = bindingMap.get(commandName);
        shortcutName = commandBinding ? commandBinding.keySeq : '';
      }
      const actionDesc = Msgs.getMsg(actionMsg);
      actionsMenu.addMenuItem(
          actionDesc, shortcutName, '' /* menuItemBraille */, '' /* gesture */,
          () => BackgroundBridge.PanelBackground
                    .performStandardActionOnCurrentNode(standardAction));
    }

    for (const customAction of actions.customActions) {
      actionsMenu.addMenuItem(
          customAction.description, '' /* menuItemShortcut */,
          '' /* menuItemBraille */, '' /* gesture */,
          () =>
              BackgroundBridge.PanelBackground.performCustomActionOnCurrentNode(
                  customAction.id));
    }
  }

  /**
   * Create a new menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  addMenu(menuMsg) {
    const menu = new PanelMenu(menuMsg);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */), false);
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event), false);
    $('menus_background').appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    return menu;
  }

  /**
   * @param {!KeyBinding} binding
   * @param {PanelMenu} menu
   * @param {boolean} isTouchScreen
   */
  addMenuItemFromKeyBinding(binding, menu, isTouchScreen) {
    if (!binding.title || !menu) {
      return;
    }

    const gestures = Object.keys(GestureCommandData.GESTURE_COMMAND_MAP);
    let keyText;
    let brailleText;
    let gestureText;
    if (isTouchScreen) {
      const gestureData = Object.values(GestureCommandData.GESTURE_COMMAND_MAP);
      const data = gestureData.find(data => data.command === binding.command);
      if (data) {
        gestureText = Msgs.getMsg(data.msgId);
      }
    } else {
      keyText = binding.keySeq;
      brailleText = BrailleCommandData.getDotShortcut(binding.command, true);
    }

    menu.addMenuItem(
        binding.title, keyText, brailleText, gestureText,
        () => BackgroundBridge.CommandHandler.onCommand(binding.command),
        binding.command);
  }

  /**
   * Create a new node menu with the given name and add it to the menu bar.
   * @param {!PanelNodeMenuData} menuData The title/predicate for the new menu.
   */
  addNodeMenu(menuData) {
    const menu = new PanelNodeMenu(menuData.titleId);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */));
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event));
    $('menus_background').appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    this.nodeMenuDictionary_[menuData.menuId] = menu;
  }

  /** @param {!PanelNodeMenuItemData} itemData */
  addNodeMenuItem(itemData) {
    this.nodeMenuDictionary_[itemData.menuId].addItemFromData(itemData);
  }

  /** @param {!PanelMenu} menu */
  async addOSKeyboardShortcutsMenuItem(menu) {
    let localizedSlash =
        await AsyncUtil.getLocalizedDomKeyStringForKeyCode(KeyCode.OEM_2);
    if (!localizedSlash) {
      localizedSlash = '/';
    }
    menu.addMenuItem(
        Msgs.getMsg('open_keyboard_shortcuts_menu'),
        `Ctrl+Alt+${localizedSlash}`, '', '', async () => {
          EventGenerator.sendKeyPress(
              KeyCode.OEM_2 /* forward slash */, {'ctrl': true, 'alt': true});
        });
  }

  /**
   * Create a new search menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  addSearchMenu(menuMsg) {
    this.searchMenu_ = new PanelSearchMenu(menuMsg);
    // Add event listeners to search bar.
    this.searchMenu_.searchBar.addEventListener(
        'input', event => this.onSearchBarQuery(event), false);
    this.searchMenu_.searchBar.addEventListener('mouseup', event => {
      // Clicking in the panel causes us to either activate an item or close the
      // menus altogether. Prevent that from happening if we click the search
      // bar.
      event.preventDefault();
      event.stopPropagation();
    }, false);

    $('menu-bar').appendChild(this.searchMenu_.menuBarItemElement);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseover',
        () =>
            this.activateMenu(this.searchMenu_, false /* activateFirstItem */),
        false);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(this.searchMenu_, event),
        false);
    $('menus_background').appendChild(this.searchMenu_.menuContainerElement);
    this.menus_.push(this.searchMenu_);
    return this.searchMenu_;
  }

  /** @param {!PanelMenu} touchMenu */
  addTouchGestureMenuItems(touchMenu) {
    const touchGestureItems = [];
    for (const data of Object.values(GestureCommandData.GESTURE_COMMAND_MAP)) {
      const command = data.command;
      if (!command) {
        continue;
      }

      const gestureText = Msgs.getMsg(data.msgId);
      const msgForCmd = data.commandDescriptionMsgId ||
          CommandStore.messageForCommand(command);
      let titleText;
      if (msgForCmd) {
        titleText = Msgs.getMsg(msgForCmd);
      } else {
        console.error('No localization for: ' + command + ' (gesture)');
        titleText = '';
      }
      touchGestureItems.push({titleText, gestureText, command});
    }

    touchGestureItems.sort(
        (item1, item2) => item1.titleText.localeCompare(item2.titleText));

    for (const item of touchGestureItems) {
      touchMenu.addMenuItem(
          item.titleText, '', '', item.gestureText,
          () => BackgroundBridge.CommandHandler.onCommand(item.command),
          item.command);
    }
  }

  /**
   * Advance the index of the current active menu by |delta|.
   * @param {number} delta The number to add to the active menu index.
   */
  advanceActiveMenuBy(delta) {
    let activeIndex = this.menus_.findIndex(menu => menu === this.activeMenu_);

    if (activeIndex >= 0) {
      activeIndex += delta;
      activeIndex = (activeIndex + this.menus_.length) % this.menus_.length;
    } else {
      if (delta >= 0) {
        activeIndex = 0;
      } else {
        activeIndex = this.menus_.length - 1;
      }
    }

    activeIndex = this.findEnabledMenuIndex(activeIndex, delta > 0 ? 1 : -1);
    if (activeIndex === -1) {
      return;
    }

    this.activateMenu(this.menus_[activeIndex], true /* activateFirstItem */);
  }

  /**
   * Clear any previous menus. The menus are all regenerated each time the
   * menus are opened.
   */
  clearMenus() {
    while (this.menus_.length) {
      const menu = this.menus_.pop();
      $('menu-bar').removeChild(menu.menuBarItemElement);
      $('menus_background').removeChild(menu.menuContainerElement);
    }
    if (this.activeMenu_) {
      this.lastMenu_ = this.activeMenu_.menuMsg;
    }
    this.activeMenu_ = null;
  }

  /** Disables menu items that are prohibited without a signed-in user. */
  denySignedOut() {
    for (const menu of this.menus_) {
      for (const item of menu.items) {
        if (CommandStore.denySignedOut(
                /** @type {!Command} */ (item.element.id))) {
          item.disable();
        }
      }
    }
  }

  /**
   * Starting at |startIndex|, looks for an enabled menu.
   * @param {number} startIndex
   * @param {number} delta
   * @return {number} The index of the enabled menu. -1 if not found.
   */
  findEnabledMenuIndex(startIndex, delta) {
    const endIndex = (delta > 0) ? this.menus_.length : -1;
    while (startIndex !== endIndex) {
      if (this.menus_[startIndex].enabled) {
        return startIndex;
      }
      startIndex += delta;
    }
    return -1;
  }

  /**
   * Get the callback for whatever item is currently selected.
   * @return {?Function} The callback for the current item.
   */
  getCallbackForCurrentItem() {
    if (this.activeMenu_) {
      return this.activeMenu_.getCallbackForCurrentItem();
    }
    return null;
  }

  /**
   * @param {string|undefined} opt_menuTitle
   * @return {!PanelMenu}
   */
  getSelectedMenu(opt_menuTitle) {
    const specifiedMenu =
        this.menus_.find(menu => menu.menuMsg === opt_menuTitle);
    return specifiedMenu || this.searchMenu_ || this.menus_[0];
  }

  /**
   * @return {!Promise<!Array<!KeyBinding>>}
   */
  async getSortedKeyBindings() {
    // TODO(accessibility): Commands should be based off of CommandStore and
    // not the keymap. There are commands that don't have a key binding (e.g.
    // commands for touch).
    const keymap = KeyMap.get();

    const sortedBindings = keymap.bindings().slice();
    for (const binding of sortedBindings) {
      const command = binding.command;
      const keySeq = binding.sequence;
      binding.keySeq = await KeyUtil.keySequenceToString(keySeq, true);
      const titleMsgId = CommandStore.messageForCommand(command);
      if (!titleMsgId) {
        // Title messages are intentionally missing for some keyboard
        // shortcuts.
        if (!(command in COMMANDS_WITH_NO_MSG_ID) &&
            !MenuManager.disableMissingMsgsErrorsForTesting) {
          console.error('No localization for: ' + command);
        }
        binding.title = '';
        continue;
      }
      const title = Msgs.getMsg(titleMsgId);
      binding.title = StringUtil.toTitleCase(title);
    }
    sortedBindings.sort(
        (binding1, binding2) =>
            binding1.title.localeCompare(String(binding2.title)));
    return sortedBindings;
  }

  /**
   * @param {!PanelMenu} actionsMenu
   * @param {!PanelMenu} chromevoxMenu
   * @param {!PanelMenu} jumpMenu
   * @param {!PanelMenu} speechMenu
   * @return {!Object<!CommandCategory, ?PanelMenu>}
   */
  makeCategoryMapping(actionsMenu, chromevoxMenu, jumpMenu, speechMenu) {
    return {
      [CommandCategory.ACTIONS]: actionsMenu,
      [CommandCategory.BRAILLE]: null,
      [CommandCategory.CONTROLLING_SPEECH]: speechMenu,
      [CommandCategory.DEVELOPER]: null,
      [CommandCategory.HELP_COMMANDS]: chromevoxMenu,
      [CommandCategory.INFORMATION]: speechMenu,
      [CommandCategory.JUMP_COMMANDS]: jumpMenu,
      [CommandCategory.MODIFIER_KEYS]: chromevoxMenu,
      [CommandCategory.NAVIGATION]: jumpMenu,
      [CommandCategory.NO_CATEGORY]: null,
      [CommandCategory.OVERVIEW]: jumpMenu,
      [CommandCategory.TABLES]: jumpMenu,
    };
  }
  /**
   * @param {!Array<!KeyBinding>} sortedBindings
   * @return {!Map<!Command, !KeyBinding>}
   */
  makeBindingMap(sortedBindings) {
    const bindingMap = new Map();
    for (const binding of sortedBindings) {
      bindingMap.set(binding.command, binding);
    }
    return bindingMap;
  }

  /**
   * Activate a menu whose title has been clicked. Stop event propagation at
   * this point so we don't close the ChromeVox menus and restore focus.
   * @param {PanelMenu} menu The menu we would like to activate.
   * @param {Event} mouseUpEvent The mouseup event.
   */
  onMouseUpOnMenuTitle(menu, mouseUpEvent) {
    this.activateMenu(menu, true /* activateFirstItem */);
    mouseUpEvent.preventDefault();
    mouseUpEvent.stopPropagation();
  }

  /**
   * Open / show the ChromeVox Menus.
   * @param {Event=} opt_event An optional event that triggered this.
   * @param {string=} opt_activateMenuTitle Title msg id of menu to open.
   */
  async onOpenMenus(opt_event, opt_activateMenuTitle) {
    // If the menu was already open, close it now and exit early.
    if (PanelInterface.instance.mode !== PanelMode.COLLAPSED) {
      PanelInterface.instance.setMode(PanelMode.COLLAPSED);
      return;
    }

    // Eat the event so that a mousedown isn't turned into a drag, allowing
    // users to click-drag-release to select a menu item.
    if (opt_event) {
      opt_event.stopPropagation();
      opt_event.preventDefault();
    }

    await BackgroundBridge.PanelBackground.saveCurrentNode();
    PanelInterface.instance.setMode(PanelMode.FULLSCREEN_MENUS);

    // The panel does not get focus immediately when we request to be full
    // screen (handled in ChromeVoxPanel natively on hash changed). Wait, if
    // needed, for focus to begin initialization.
    if (!document.hasFocus()) {
      await waitForWindowFocus();
    }

    const eventSource = await BackgroundBridge.EventSource.get();
    const touchScreen = (eventSource === EventSourceType.TOUCH_GESTURE);

    // Build the top-level menus.
    const searchMenu = this.addSearchMenu('panel_search_menu');
    const jumpMenu = this.addMenu('panel_menu_jump');
    const speechMenu = this.addMenu('panel_menu_speech');
    const touchMenu =
        touchScreen ? this.addMenu('panel_menu_touchgestures') : null;
    const chromevoxMenu = this.addMenu('panel_menu_chromevox');
    const actionsMenu = this.addMenu('panel_menu_actions');

    // Add a menu item that opens the full list of ChromeBook keyboard
    // shortcuts. We want this to be at the top of the ChromeVox menu.
    await this.addOSKeyboardShortcutsMenuItem(chromevoxMenu);

    // Create a mapping between categories from CommandStore, and our
    // top-level menus. Some categories aren't mapped to any menu.
    const categoryToMenu = this.makeCategoryMapping(
        actionsMenu, chromevoxMenu, jumpMenu, speechMenu);

    // Make a copy of the key bindings, get the localized title of each
    // command, and then sort them.
    const sortedBindings = await this.getSortedKeyBindings();

    // Insert items from the bindings into the menus.
    const bindingMap = this.makeBindingMap(sortedBindings);
    for (const binding of bindingMap.values()) {
      const category = CommandStore.categoryForCommand(binding.command);
      const menu = category ? categoryToMenu[category] : null;
      this.addMenuItemFromKeyBinding(binding, menu, touchScreen);
    }

    // Add Touch Gestures menu items.
    if (touchMenu) {
      this.addTouchGestureMenuItems(touchMenu);
    }

    if (PanelInterface.instance.sessionState !== 'IN_SESSION') {
      this.denySignedOut();
    }

    // Add a menu item that disables / closes ChromeVox.
    chromevoxMenu.addMenuItem(
        Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', '', '',
        async () => PanelInterface.instance.onClose());

    for (const menuData of ALL_PANEL_MENU_NODE_DATA) {
      this.addNodeMenu(menuData);
    }
    await BackgroundBridge.PanelBackground.createAllNodeMenuBackgrounds(
        opt_activateMenuTitle);

    await this.addActionsMenuItems(actionsMenu, bindingMap);

    // Activate either the specified menu or the search menu.
    const selectedMenu = this.getSelectedMenu(opt_activateMenuTitle);

    const activateFirstItem = (selectedMenu !== this.searchMenu);
    this.activateMenu(selectedMenu, activateFirstItem);
  }

  /**
   * Listens to changes in the menu search bar. Populates the search menu
   * with items that match the search bar's contents.
   * Note: we ignore PanelNodeMenu items and items without shortcuts.
   * @param {Event} event The input event.
   */
  onSearchBarQuery(event) {
    if (!this.searchMenu_) {
      throw Error('MenuManager.searchMenu_ must be defined');
    }
    const query = event.target.value.toLowerCase();
    this.searchMenu_.clear();
    // Show the search results menu.
    this.activateMenu(this.searchMenu_, false /* activateFirstItem */);
    // Populate.
    if (query) {
      for (const menu of this.menus_) {
        if (menu === this.searchMenu_ || menu instanceof PanelNodeMenu) {
          continue;
        }
        for (const item of menu.items) {
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
            this.searchMenu_.copyAndAddMenuItem(item);
          }
        }
      }
    }

    if (this.searchMenu_.items.length === 0) {
      this.searchMenu_.addMenuItem(
          Msgs.getMsg('panel_menu_item_none'), '', '', '', function() {});
    }
    this.searchMenu_.activateItem(0);
  }

  // The following getters and setters are temporary during the migration from
  // panel.js.

  /** @return {?PanelMenu} */
  get activeMenu() {
    return this.activeMenu_;
  }
  /** @param {?PanelMenu} menu */
  set activeMenu(menu) {
    this.activeMenu_ = menu;
  }

  /** @return {string} */
  get lastMenu() {
    return this.lastMenu_;
  }
  /** @param {string} menuMsg */
  set lastMenu(menuMsg) {
    this.lastMenu_ = menuMsg;
  }

  /** @return {!Array<!PanelMenu>} */
  get menus() {
    return this.menus_;
  }

  /** @return {!Object<!PanelNodeMenuId, !PanelNodeMenu>} */
  get nodeMenuDictionary() {
    return this.nodeMenuDictionary_;
  }

  /** @return {?PanelSearchMenu} */
  get searchMenu() {
    return this.searchMenu_;
  }
  /** @param {?PanelSearchMenu} menu */
  set searchMenu(menu) {
    this.searchMenu_ = menu;
  }
}


/** @type {boolean} */
MenuManager.disableMissingMsgsErrorsForTesting = false;

// Local to module.

const COMMANDS_WITH_NO_MSG_ID = [
  'nativeNextCharacter',
  'nativePreviousCharacter',
  'nativeNextWord',
  'nativePreviousWord',
  'enableLogging',
  'disableLogging',
  'dumpTree',
  'showActionsMenu',
  'enableChromeVoxArcSupportForCurrentApp',
  'disableChromeVoxArcSupportForCurrentApp',
  'showTalkBackKeyboardShortcuts',
  'copy',
];

const ACTION_TO_MSG_ID = {
  decrement: 'action_decrement_description',
  doDefault: 'perform_default_action',
  increment: 'action_increment_description',
  scrollBackward: 'action_scroll_backward_description',
  scrollForward: 'action_scroll_forward_description',
  showContextMenu: 'show_context_menu',
  longClick: 'force_long_click_on_current_item',
};

async function waitForWindowFocus() {
  return new Promise(
      resolve => window.addEventListener('focus', resolve, {once: true}));
}
