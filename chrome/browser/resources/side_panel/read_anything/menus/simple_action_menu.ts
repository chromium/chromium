// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '../read_anything_toolbar.css.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {emitEvent, openMenu} from '../common.js';
import type {ToolbarEvent} from '../common.js';

import type {MenuStateItem} from './menu_util.js';
import {getTemplate} from './simple_action_menu.html.js';

export interface SimpleActionMenu {
  $: {
    lazyMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SimpleActionMenuBase = WebUiListenerMixin(PolymerElement);

// Represents a simple dropdown menu that contains a flat list of items with
// text and an optional icon. Selecting an item in this menu propagates that
// event, sets that item as selected with a visual checkmark, and then closes
// the menu.
export class SimpleActionMenu extends SimpleActionMenuBase {
  currentSelectedIndex: number;
  readonly menuItems: Array<MenuStateItem<any>>;
  readonly eventName: ToolbarEvent;

  static get is() {
    return 'simple-action-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentSelectedIndex: Number,
      menuItems: Array,
      eventName: String,
    };
  }

  open(anchor: HTMLElement) {
    openMenu(this.$.lazyMenu.get(), anchor);
  }

  private onClick_(event: DomRepeatEvent<MenuStateItem<any>>) {
    this.currentSelectedIndex = event.model.index;
    emitEvent(this, this.eventName, {data: event.model.item.data});
    this.$.lazyMenu.get().close();
  }

  private isItemSelected_(index: number): boolean {
    return index === this.currentSelectedIndex;
  }

  private doesItemHaveIcon_(item: MenuStateItem<any>): boolean {
    return item.icon !== undefined;
  }

  private itemIcon_(item: MenuStateItem<any>): string {
    return item.icon === undefined ? '' : item.icon;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'simple-action-menu': SimpleActionMenu;
  }
}

customElements.define(SimpleActionMenu.is, SimpleActionMenu);
