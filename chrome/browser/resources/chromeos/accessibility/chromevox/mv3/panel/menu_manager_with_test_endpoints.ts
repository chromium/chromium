// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BridgeHelper} from '/common/bridge_helper.js';

import {BridgeConstants} from '../common/bridge_constants.js';
import {KeyMap} from '../common/key_map.js';
import {Msgs} from '../common/msgs.js';

import {MenuManager} from './menu_manager.js';
import {PanelInterface} from './panel_interface.js';

const TARGET = BridgeConstants.PanelTest.TARGET;
const Action = BridgeConstants.PanelTest.Action;

export class MenuManagerWithTestEndpoints extends MenuManager {
  constructor() {
    super();

    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_ACTIVE_MENU_TEST,
        () => this.performActiveMenuTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_ADD_MENU_TEST, () => this.performAddMenuTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_ADD_NODE_MENU_TEST,
        () => this.performAddNodeTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_ADVANCE_ACTIVE_MENU_BY_TEST,
        () => this.performAdvanceActiveMenuByTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_CLEAR_MENUS_TEST,
        () => this.performClearMenusTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_DENY_SIGNED_OUT_TEST,
        () => this.performDenySignedOutTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_FIND_ENABLED_MENU_INDEX_TEST,
        () => this.performFindEnabledMenuIndexTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_GET_SORTED_KEY_BINDINGS_TEST,
        () => this.performGetSortedKeyBindingTest_());
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_ON_SEARCH_BAR_QUERY_TEST,
        () => this.performOnSearchBarQueryTest_());
  }

  private performActiveMenuTest_(): string {
    const searchMenu = this.activeMenu_;
    const chromevoxMenu = this.getSelectedMenu('panel_menu_chromevox');
    if ('panel_menu_chromevox' != chromevoxMenu.menuMsg) {
      return `'panel_menu_chromevox' != chromevoxMenu.menuMsg`;
    }

    // Set a pending callback so we can tell if it was cleared.
    // @ts-ignore
    PanelInterface.instance!.setPendingCallback(() => {});
    // @ts-ignore
    if (!PanelInterface.instance!.pendingCallback_) {
      return '`PanelInterface.instance!.pendingCallback_` is null or undefined';
    }

    this.activateMenu(chromevoxMenu, false);

    if (chromevoxMenu !== this.activeMenu_) {
      return 'chromevoxMenu !== this.activeMenu_';
    }
    // @ts-ignore
    if (-1 !== searchMenu!.activeIndex_) {
      return '-1 !== searchMenu.activeIndex_';
    }

    // Confirm the pending callback was cleared.
    // @ts-ignore
    if (PanelInterface.instance!.pendingCallback_) {
      return '`PanelInterface.instance!.pendingCallback_` is not null or undefined';
    }

    return 'pass';
  }

  private performAddMenuTest_(): string {
    // Msgs.getMsg() throws an error if the message is not a translated string.
    // Mock it out to avoid the error.
    Msgs.getMsg = () => '';

    const data = {menuId: 6, titleId: 'fake_node'};
    // @ts-ignore
    this.addNodeMenu(data);

    const fakeMenu = this.getSelectedMenu('fake_node');
    // Ensure we didn't receive the default menu.
    if ('fake_node' !== fakeMenu.menuMsg) {
      return `'fake_node' !== fakeMenu.menuMsg`;
    }
    // @ts-ignore
    if (fakeMenu !== this.nodeMenuDictionary_[6]) {
      return 'fakeMenu !== this.getMenuManager().nodeMenuDictionary_[6]';
    }

    return 'pass';
  }

  private performAddNodeTest_(): string {
    // Msgs.getMsg() throws an error if the message is not a translated string.
    // Mock it out to avoid the error.
    Msgs.getMsg = () => '';

    const data = {menuId: 6, titleId: 'fake_node'};
    // @ts-ignore
    this.addNodeMenu(data);
    const fakeMenu = this.getSelectedMenu('fake_node');

    // Ensure we didn't receive the default menu.
    if ('fake_node' !== fakeMenu.menuMsg) {
      return `'fake_node' !== fakeMenu.menuMsg`;
    }

    // @ts-ignore
    if (fakeMenu !== this.nodeMenuDictionary_[6]) {
      return 'fakeMenu !== this.nodeMenuDictionary_[6]';
    }

    return 'pass';
  }

  private performAdvanceActiveMenuByTest_(): string {
    const activeIndex = () =>
        this.menus_.findIndex(menu => menu === this.activeMenu_);

    if (0 !== activeIndex()) {
      return '0 !== activeIndex()';
    }

    // Forward with an active menu.
    this.advanceActiveMenuBy(2);
    if (2 !== activeIndex()) {
      return '2 !== activeIndex()';
    }

    // Backward with an active menu.
    this.advanceActiveMenuBy(-1);
    if (1 !== activeIndex()) {
      return '1 !== activeIndex()';
    }

    // Wrap around backwards.
    this.advanceActiveMenuBy(-3);
    if (this.menus_.length - 2 !== activeIndex()) {
      return 'this.menus_.length - 2 !== activeIndex()';
    }

    // Wrap around forwards.
    this.advanceActiveMenuBy(4);
    if (2 !== activeIndex()) {
      return '2 !== activeIndex()';
    }

    // Forward with no active menu.
    this.activeMenu_ = null;
    if (-1 !== activeIndex()) {
      return '-1 !== activeIndex()';
    }

    this.advanceActiveMenuBy(1);
    if (0 !== activeIndex()) {
      return '0 !== activeIndex()';
    }

    // Backward with no active menu.
    this.activeMenu_ = null;
    if (-1 !== activeIndex()) {
      return '-1 !== activeIndex()';
    }

    this.advanceActiveMenuBy(-1);
    if (this.menus_.length - 1 !== activeIndex()) {
      return 'this.menus_.length - 1 !== activeIndex()';
    }

    return 'pass';
  }

  private performClearMenusTest_(): string {
    const menuBar = document.getElementById('menu-bar');
    if (0 === menuBar!.children.length) {
      return '0 === menuBar.children.length';
    }
    if ('' !== this.lastMenu_) {
      return `'' !== this.lastMenu_`;
    }
    if (null === this.activeMenu_) {
      return 'null === this.activeMenu_';
    }

    this.clearMenus();

    if (0 !== menuBar!.children.length) {
      return '0 !== menuBar.children.length';
    }
    // @ts-ignore
    if ('panel_search_menu' !== this.lastMenu_) {
      return `'panel_search_menu' !== this.lastMenu_`;
    }
    if (null !== this.activeMenu_) {
      return 'null !== this.activeMenu_';
    }

    return 'pass';
  }

  private performDenySignedOutTest_(): string {
    const someItemsAreDisabled = () =>
        // @ts-ignore
        this.menus_.some(menu => menu.items.some(item => !item.enabled_));

    if (someItemsAreDisabled()) {
      return 'someItemsAreDisabled()';
    }

    this.denySignedOut();
    if (!someItemsAreDisabled()) {
      return '!someItemsAreDisabled()';
    }

    return 'pass';
  }

  private performFindEnabledMenuIndexTest_(): string {
    // Try forward and backward when all menus are enabled.
    let index = this.findEnabledMenuIndex(0, 1);
    if (0 !== index) {
      return '0 !== index';
    }

    index = this.findEnabledMenuIndex(1, -1);
    if (1 !== index) {
      return '1 !== index';
    }

    // Try forward and backward when no menus are enabled.
    // @ts-ignore
    this.menus_.forEach(menu => menu.enabled_ = false);

    index = this.findEnabledMenuIndex(2, 1);
    if (-1 !== index) {
      return '-1 !== index';
    }

    index = this.findEnabledMenuIndex(2, -1);
    if (-1 !== index) {
      return '-1 !== index';
    }

    // Try forward and backward when one menu is enabled in that direction.
    // @ts-ignore
    this.menus_[2].enabled_ = true;

    index = this.findEnabledMenuIndex(0, 1);
    if (2 !== index) {
      return '2 !== index';
    }

    index = this.findEnabledMenuIndex(this.menus_.length - 1, -1);
    if (2 !== index) {
      return '2 !== index';
    }

    // Try forward and backward when there is one enabled menu, but in the
    // opposite direction.
    index = this.findEnabledMenuIndex(3, 1);
    if (-1 !== index) {
      return '-1 !== index';
    }

    index = this.findEnabledMenuIndex(1, -1);
    if (-1 !== index) {
      return '-1 !== index';
    }

    return 'pass';
  }

  private async performGetSortedKeyBindingTest_(): Promise<string> {
    const keymapBindings = KeyMap.get().bindings();
    const sortedKeyBindings = await this.getSortedKeyBindings();
    if (keymapBindings.length !== sortedKeyBindings.length) {
      return 'keymapBindings.length !== sortedKeyBindings.length';
    }

    for (const binding of sortedKeyBindings) {
      // @ts-ignore
      if ('' === binding.command) {
        return `'' === binding.command`;
      }
      if (typeof binding.keySeq !== 'string') {
        return `typeof binding.keySeq !== 'string'`;
      }
      if (typeof binding.title !== 'string') {
        return `typeof binding.title !== 'string'`;
      }
      if (!keymapBindings.some(
              keyBinding => binding.sequence.equals(keyBinding.sequence))) {
        return '!keymapBindings.some(keyBinding => binding.sequence.equals(keyBinding.sequence)';
      }
    }

    return 'pass';
  }

  private async performOnSearchBarQueryTest_(): Promise<string> {
    function expectMethodCall(
        object: {[k: string]: any}, method: string): Promise<void> {
      let methodCalled: Promise<void> = new Promise(resolve => {
        const original = object[method].bind(object);
        object[method] = async (...args: any[]) => {
          await original(...args);
          resolve();
        };
      });
      return methodCalled;
    }

    const expectSearchMenuCleared =
        expectMethodCall(this.searchMenu_!, 'clear');
    const expectActivateMenu = expectMethodCall(this, 'activateMenu');

    // @ts-ignore
    this.onSearchBarQuery({target: {value: 'a'}});

    await expectSearchMenuCleared;
    await expectActivateMenu;

    if (0 === this.searchMenu_!.items.length) {
      return '0 === this.searchMenu_.items.length';
    }

    return 'pass';
  }
}
