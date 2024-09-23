// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {StringUtil} from '/common/string_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {BrailleCommandData} from '../common/braille/braille_command_data.js';
import {Command, CommandCategory} from '../common/command.js';
import {CommandStore} from '../common/command_store.js';
import {EventSourceType} from '../common/event_source_type.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {KeyMap} from '../common/key_map.js';
import {KeyBinding} from '../common/key_sequence.js';
import {KeyUtil} from '../common/key_util.js';
import {Msgs} from '../common/msgs.js';
import {ALL_PANEL_MENU_NODE_DATA, PanelNodeMenuData, PanelNodeMenuId, PanelNodeMenuItemData} from '../common/panel_menu_data.js';

import {PanelInterface} from './panel_interface.js';
import {PanelMenu, PanelNodeMenu, PanelSearchMenu} from './panel_menu.js';
import {PanelMode} from './panel_mode.js';

const $ = (id: string): HTMLElement | null => document.getElementById(id);

interface TouchMenuData {
  titleText: string;
  gestureText: string;
  command: Command;
}

export class MenuManager {
  private activeMenu_: PanelMenu | null = null;
  private lastMenu_ = '';
  private menus_: PanelMenu[] = [];
  private nodeMenuDictionary_:
      Partial<Record<PanelNodeMenuId, PanelNodeMenu>> = {};
  private searchMenu_: PanelSearchMenu | null = null;

  static disableMissingMsgsErrorsForTesting = false;

  /**
   * Activate a menu, which implies hiding the previous active menu.
   * @param menu The new menu to activate.
   * @param activateFirstItem Whether or not we should activate the
   *     menu's first item.
   */
  activateMenu(menu: PanelMenu | null, activateFirstItem: boolean): void {
    if (menu === this.activeMenu_) {
      return;
    }

    if (this.activeMenu_) {
      this.activeMenu_.deactivate();
      this.activeMenu_ = null;
    }

    this.activeMenu_ = menu;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    PanelInterface.instance!.setPendingCallback(null);

    if (this.activeMenu_) {
      this.activeMenu_.activate(activateFirstItem);
    }
  }

  async addActionsMenuItems(
      actionsMenu: PanelMenu,
      bindingMap: Map<Command, KeyBinding>): Promise<void> {
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
        shortcutName = commandBinding ? commandBinding.keySeq as string : '';
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
   * @param menuMsg The msg id of the new menu to add.
   * @return The menu just created.
   */
  addMenu(menuMsg: string): PanelMenu {
    const menu = new PanelMenu(menuMsg);
    $('menu-bar')!.appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */), false);
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event), false);
    $('menus_background')!.appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    return menu;
  }

  addMenuItemFromKeyBinding(
      binding: KeyBinding, menu: PanelMenu | null,
      isTouchScreen: boolean): void {
    if (!binding.title || !menu) {
      return;
    }

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
   * @param menuData The title/predicate for the new menu.
   */
  addNodeMenu(menuData: PanelNodeMenuData): void {
    const menu = new PanelNodeMenu(menuData.titleId);
    $('menu-bar')!.appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */));
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event));
    $('menus_background')!.appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    this.nodeMenuDictionary_[menuData.menuId] = menu;
  }

  addNodeMenuItem(itemData: PanelNodeMenuItemData): void {
    this.nodeMenuDictionary_[itemData.menuId]?.addItemFromData(itemData);
  }

  /**
   * Create a new search menu with the given name and add it to the menu bar.
   * @param menuMsg The msg id of the new menu to add.
   * @return The menu just created.
   */
  addSearchMenu(menuMsg: string): PanelMenu {
    this.searchMenu_ = new PanelSearchMenu(menuMsg);
    // Add event listeners to search bar.
    this.searchMenu_.searchBar.addEventListener(
        'input',
        (event: Event) => this.onSearchBarQuery(event as InputEvent), false);
    this.searchMenu_.searchBar.addEventListener('mouseup', event => {
      // Clicking in the panel causes us to either activate an item or close the
      // menus altogether. Prevent that from happening if we click the search
      // bar.
      event.preventDefault();
      event.stopPropagation();
    }, false);

    $('menu-bar')!.appendChild(this.searchMenu_.menuBarItemElement);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseover',
        () =>
            this.activateMenu(this.searchMenu_, false /* activateFirstItem */),
        false);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(this.searchMenu_!, event),
        false);
    $('menus_background')!.appendChild(this.searchMenu_.menuContainerElement);
    this.menus_.push(this.searchMenu_);
    return this.searchMenu_;
  }

  addTouchGestureMenuItems(touchMenu: PanelMenu): void {
    const touchGestureItems: TouchMenuData[] = [];
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
   * @param delta The number to add to the active menu index.
   */
  advanceActiveMenuBy(delta: number): void {
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
   * Advance the index of the current active menu item by |delta|.
   * @param delta The number to add to the active menu item index.
   */
  advanceItemBy(delta: number): void {
    if (this.activeMenu_) {
      this.activeMenu_.advanceItemBy(delta);
    }
  }

  /**
   * Clear any previous menus. The menus are all regenerated each time the
   * menus are opened.
   */
  clearMenus(): void {
    while (this.menus_.length) {
      const menu = this.menus_.pop();
      $('menu-bar')!.removeChild(menu!.menuBarItemElement);
      $('menus_background')!.removeChild(menu!.menuContainerElement);

      if (this.activeMenu_) {
        this.lastMenu_ = this.activeMenu_.menuMsg;
      }
      this.activeMenu_ = null;
    }
  }

  /** Disables menu items that are prohibited without a signed-in user. */
  denySignedOut(): void {
    for (const menu of this.menus_) {
      for (const item of menu.items) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        if (CommandStore.denySignedOut(item.element!.id as Command)) {
          item.disable();
        }
      }
    }
  }

  /**
   * Starting at |startIndex|, looks for an enabled menu.
   * @return The index of the enabled menu. -1 if not found.
   */
  findEnabledMenuIndex(startIndex: number, delta: number): number {
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
   * @return The callback for the current item.
   *
   * TODO(b/267329383): Specify this as Promise<void> once PanelMenu
   * is converted to typescript.
   */
  getCallbackForCurrentItem(): (() => Promise<any>) | null{
    if (this.activeMenu_) {
      return this.activeMenu_.getCallbackForCurrentItem();
    }
    return null;
  }

  getSelectedMenu(menuTitle?: string): PanelMenu {
    const specifiedMenu =
        this.menus_.find(menu => menu.menuMsg === menuTitle);
    return specifiedMenu || this.searchMenu_ || this.menus_[0];
  }

  async getSortedKeyBindings(): Promise<KeyBinding[]> {
    // TODO(accessibility): Commands should be based off of CommandStore and
    // not the keymap. There are commands that don't have a key binding (e.g.
    // commands for touch).
    const keymap = KeyMap.get();

    // A shallow copy of the bindings is returned, so re-ordering the elements
    // does not change the original.
    const sortedBindings = keymap.bindings();
    for (const binding of sortedBindings) {
      const command = binding.command;
      const keySeq = binding.sequence;
      binding.keySeq = await KeyUtil.keySequenceToString(keySeq, true);
      const titleMsgId = CommandStore.messageForCommand(command);
      if (!titleMsgId) {
        // Title messages are intentionally missing for some keyboard shortcuts.
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
            binding1.title!.localeCompare(String(binding2.title)));
    return sortedBindings;
  }

  makeBindingMap(sortedBindings: KeyBinding[]): Map<Command, KeyBinding> {
    const bindingMap = new Map();
    for (const binding of sortedBindings) {
      bindingMap.set(binding.command, binding);
    }
    return bindingMap;
  }

  makeCategoryMapping(
      actionsMenu: PanelMenu, chromevoxMenu: PanelMenu, jumpMenu: PanelMenu,
      speechMenu: PanelMenu): Record<CommandCategory, PanelMenu|null> {
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

  /** @return True if the event was handled. */
  onKeyDown(event: KeyboardEvent): boolean {
    if (!this.activeMenu) {
      return false;
    }

    if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
      return false;
    }

    // We need special logic for navigating the search bar.
    // If left/right arrow are pressed, we should adjust the search bar's
    // cursor. We only want to advance the active menu if we are at the
    // beginning/end of the search bar's contents.
    if (this.searchMenu_ && event.target === this.searchMenu_.searchBar) {
      const input = event.target as HTMLInputElement;
      switch (event.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          if (input.value) {
            // TODO(b/314203187): Not null asserted, check that this is correct.
            const cursorIndex =
                input.selectionStart! + (event.key === 'ArrowRight' ? 1 : -1);
            const queryLength = input.value.length;
            if (cursorIndex >= 0 && cursorIndex <= queryLength) {
              return false;
            }
          }
          break;
        case ' ':
          return false;
      }
    }

    switch (event.key) {
      case 'ArrowLeft':
        this.advanceActiveMenuBy(-1);
        break;
      case 'ArrowRight':
        this.advanceActiveMenuBy(1);
        break;
      case 'ArrowUp':
        this.advanceItemBy(-1);
        break;
      case 'ArrowDown':
        this.advanceItemBy(1);
        break;
      case 'Escape':
        // TODO(b/314203187): Not null asserted, check that this is correct.
        PanelInterface.instance!.closeMenusAndRestoreFocus();
        break;
      case 'PageUp':
        this.advanceItemBy(10);
        break;
      case 'PageDown':
        this.advanceItemBy(-10);
        break;
      case 'Home':
        this.scrollToTop();
        break;
      case 'End':
        this.scrollToBottom();
        break;
      case 'Enter':
      case ' ':
        if (!this.getCallbackForCurrentItem()) {
          // If there's no callback for the current menu item, then we shouldn't
          // perform any special logic. Return false here and let the key event
          // propagate so that it can potentially be handled elsewhere.
          return false;
        }

        // TODO(b/314203187): Not null asserted, check that this is correct.
        PanelInterface.instance!.setPendingCallback(
            this.getCallbackForCurrentItem());
        PanelInterface.instance!.closeMenusAndRestoreFocus();
        break;
      default:
        // Don't mark this event as handled.
        return false;
    }
    return true;
  }

  /**
   * Called when the user releases the mouse button. If it's anywhere other
   * than on the menus button, close the menus and return focus to the page,
   * and if the mouse was released over a menu item, execute that item's
   * callback.
   */
  onMouseUp(event: MouseEvent): void {
    if (!this.activeMenu_) {
      return;
    }

    let target: HTMLElement|null = event.target as HTMLElement;
    while (target && !target.classList.contains('menu-item')) {
      // Allow the user to click and release on the menu button and leave
      // the menu button.
      if (target.id === 'menus_button') {
        return;
      }

      target = target.parentElement;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (target && this.activeMenu_) {
      PanelInterface.instance!.setPendingCallback(
          this.activeMenu_.getCallbackForElement(target));
    }
    PanelInterface.instance!.closeMenusAndRestoreFocus();
  }

  /**
   * Activate a menu whose title has been clicked. Stop event propagation at
   * this point so we don't close the ChromeVox menus and restore focus.
   * @param menu The menu we would like to activate.
   * @param mouseUpEvent The mouseup event.
   */
  onMouseUpOnMenuTitle(menu: PanelMenu, mouseUpEvent: MouseEvent): void {
    this.activateMenu(menu, true /* activateFirstItem */);
    mouseUpEvent.preventDefault();
    mouseUpEvent.stopPropagation();
  }

  /**
   * Open / show the ChromeVox Menus.
   * @param {Event=} event An optional event that triggered this.
   * @param {string=} activateMenuTitle?: string Title msg id of menu to open.
   */
  async onOpenMenus(event?: Event, activateMenuTitle?: string): Promise<void> {
    // If the menu was already open, close it now and exit early.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (PanelInterface.instance!.mode !== PanelMode.COLLAPSED) {
      PanelInterface.instance!.setMode(PanelMode.COLLAPSED);
      return;
    }

    // Eat the event so that a mousedown isn't turned into a drag, allowing
    // users to click-drag-release to select a menu item.
    if (event) {
      event.stopPropagation();
      event.preventDefault();
    }

    await BackgroundBridge.PanelBackground.saveCurrentNode();
    // TODO(b/314203187): Not null asserted, check that this is correct.
    PanelInterface.instance!.setMode(PanelMode.FULLSCREEN_MENUS);

    // The panel does not get focus immediately when we request to be full
    // screen (handled in ChromeVoxPanel natively on hash changed). Wait, if
    // needed, for focus to begin initialization.
    if (!document.hasFocus()) {
      await waitForWindowFocus();
    }

    const eventSource = await BackgroundBridge.EventSource.get();
    const touchScreen = (eventSource === EventSourceType.TOUCH_GESTURE);

    // Build the top-level menus.
    this.addSearchMenu('panel_search_menu');
    const jumpMenu = this.addMenu('panel_menu_jump');
    const speechMenu = this.addMenu('panel_menu_speech');
    const touchMenu =
        touchScreen ? this.addMenu('panel_menu_touchgestures') : null;
    const chromevoxMenu = this.addMenu('panel_menu_chromevox');
    const actionsMenu = this.addMenu('panel_menu_actions');

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

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (PanelInterface.instance!.sessionState !== 'IN_SESSION') {
      this.denySignedOut();
    }

    // Add a menu item that disables / closes ChromeVox.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    chromevoxMenu.addMenuItem(
        Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', '', '',
        async () => PanelInterface.instance!.onClose());

    for (const menuData of ALL_PANEL_MENU_NODE_DATA) {
      this.addNodeMenu(menuData);
    }
    await BackgroundBridge.PanelBackground.createAllNodeMenuBackgrounds(
        activateMenuTitle);

    await this.addActionsMenuItems(actionsMenu, bindingMap);

    // Activate either the specified menu or the search menu.
    const selectedMenu = this.getSelectedMenu(activateMenuTitle);

    const activateFirstItem = (selectedMenu !== this.searchMenu);
    this.activateMenu(selectedMenu, activateFirstItem);
  }

  /**
   * Listens to changes in the menu search bar. Populates the search menu
   * with items that match the search bar's contents.
   * Note: we ignore PanelNodeMenu items and items without shortcuts.
   * @param event The input event.
   */
  onSearchBarQuery(event: InputEvent): void {
    if (!this.searchMenu_) {
      throw Error('MenuManager.searchMenu_ must be defined');
    }
    const query = (event.target as HTMLInputElement).value.toLowerCase();
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
          Msgs.getMsg(
              'panel_menu_item_none'), '', '', '', () => Promise.resolve());
    }
    this.searchMenu_.activateItem(0);
  }

  /** Sets the index of the current active menu to be the last index. */
  scrollToBottom(): void {
    this.activeMenu_!.scrollToBottom();
  }

  /** Sets the index of the current active menu to be 0. */
  scrollToTop(): void {
    this.activeMenu_!.scrollToTop();
  }

  // The following getters and setters are temporary during the migration from
  // panel.js.

  get activeMenu(): PanelMenu | null {
    return this.activeMenu_;
  }
  set activeMenu(menu: PanelMenu | null) {
    this.activeMenu_ = menu;
  }

  get lastMenu(): string {
    return this.lastMenu_;
  }
  set lastMenu(menuMsg: string) {
    this.lastMenu_ = menuMsg;
  }

  get menus(): PanelMenu[] {
    return this.menus_;
  }

  get nodeMenuDictionary(): Partial<Record<PanelNodeMenuId, PanelNodeMenu>> {
    return this.nodeMenuDictionary_;
  }

  get searchMenu(): PanelSearchMenu | null {
    return this.searchMenu_;
  }
  set searchMenu(menu: PanelSearchMenu | null) {
    this.searchMenu_ = menu;
  }
}

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

const ACTION_TO_MSG_ID: Record<string, string> = {
  decrement: 'action_decrement_description',
  doDefault: 'perform_default_action',
  increment: 'action_increment_description',
  scrollBackward: 'action_scroll_backward_description',
  scrollForward: 'action_scroll_forward_description',
  showContextMenu: 'show_context_menu',
  longClick: 'force_long_click_on_current_item',
};

async function waitForWindowFocus(): Promise<any> {
  return new Promise(
      resolve => window.addEventListener('focus', resolve, {once: true}));
}

TestImportManager.exportForTesting(MenuManager);
