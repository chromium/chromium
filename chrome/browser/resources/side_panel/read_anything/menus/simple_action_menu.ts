// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {openMenu, ToolbarEvent} from '../common.js';

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

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      currentSelectedIndex: {type: Number},
      menuItems: {type: Array},
      eventName: {type: String},
      label: {type: String},
    };
  }

  accessor currentSelectedIndex: number = 0;
  accessor menuItems: Array<MenuStateItem<any>> = [];

  // Initializing to random value, but this is set by the parent.
  accessor eventName: ToolbarEvent = ToolbarEvent.THEME;
  accessor label: string = '';

  open(anchor: HTMLElement) {
    openMenu(this.$.lazyMenu.get(), anchor);
  }

  protected onClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    this.currentSelectedIndex =
        Number.parseInt(currentTarget.dataset['index']!);
    const menuItem = this.menuItems[this.currentSelectedIndex];
    assert(menuItem);
    this.fire(this.eventName, {data: menuItem.data});
    this.$.lazyMenu.get().close();
  }

  protected isItemSelected_(index: number): boolean {
    return index === this.currentSelectedIndex;
  }

  protected doesItemHaveIcon_(item: MenuStateItem<any>): boolean {
    return item.icon !== undefined;
  }

  protected itemIcon_(item: MenuStateItem<any>): string {
    return item.icon === undefined ? '' : item.icon;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'simple-action-menu': SimpleActionMenuElement;
  }
}

customElements.define(SimpleActionMenuElement.is, SimpleActionMenuElement);
