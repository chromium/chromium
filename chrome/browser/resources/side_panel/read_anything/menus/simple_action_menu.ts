// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ToolbarEvent} from '../content/read_anything_types.js';
import {openMenu} from '../shared/common.js';

import type {MenuStateItem} from './menu_util.js';
import {getCss} from './simple_action_menu.css.js';
import {getHtml} from './simple_action_menu.html.js';

export interface SimpleActionMenuElement {
  $: {
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

const SimpleActionMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Represents a simple dropdown menu that contains a flat list of items with
// text and an optional icon. Selecting an item in this menu propagates that
// event, sets that item as selected with a visual checkmark, and then closes
// the menu.
export class SimpleActionMenuElement extends SimpleActionMenuElementBase {
  static get is() {
    return 'simple-action-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentSelectedIndex: {type: Number},
      menuItems: {type: Array},
      eventName: {type: String},
      label: {type: String},
      nonModal: {type: Boolean},
      closeOnClick: {type: Boolean},
    };
  }

  accessor currentSelectedIndex: number = 0;
  accessor menuItems: Array<MenuStateItem<any>> = [];
  accessor nonModal: boolean = false;
  accessor closeOnClick: boolean = true;

  // Initializing to random value, but this is set by the parent.
  accessor eventName: ToolbarEvent = ToolbarEvent.THEME;
  accessor label: string = '';

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    openMenu(this.$.lazyMenu.get(), anchor, showAtConfig);
  }

  close() {
    this.$.lazyMenu.get().close();
  }

  protected onClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    this.currentSelectedIndex =
        Number.parseInt(currentTarget.dataset['index']!);
    const menuItem = this.menuItems[this.currentSelectedIndex];
    assert(menuItem);
    const eventName = menuItem.eventName || this.eventName;
    this.fire(eventName, {data: menuItem.data});
    if (this.closeOnClick) {
      this.$.lazyMenu.get().close();
    }
  }

  protected isItemSelected_(index: number, item: MenuStateItem<any>): boolean {
    // Only use currentSelectedIndex if item.selected is undefined.
    return item.selected ?? (index === this.currentSelectedIndex);
  }

  protected doesItemHaveIcon_(item: MenuStateItem<any>): boolean {
    return item.icon !== undefined;
  }

  protected itemIcon_(item: MenuStateItem<any>): string {
    return item.icon === undefined ? '' : item.icon;
  }

  protected doesItemHaveHeader_(item: MenuStateItem<any>): boolean {
    return chrome.readingMode.isLineFocusEnabled && !!item.header;
  }

  protected doesItemHaveHeaderSeparator_(item: MenuStateItem<any>): boolean {
    return chrome.readingMode.isLineFocusEnabled && !!item.header?.separator;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'simple-action-menu': SimpleActionMenuElement;
  }
}

customElements.define(SimpleActionMenuElement.is, SimpleActionMenuElement);
