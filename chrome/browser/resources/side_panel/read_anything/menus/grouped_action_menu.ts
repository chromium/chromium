// Copyright 2026 The Chromium Authors
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
import {openMenu} from '../shared/common.js';

import {getCss} from './action_menu.css.js';
import {getHtml} from './grouped_action_menu.html.js';
import type {MenuGroup} from './menu_util.js';

export interface GroupedActionMenuElement {
  $: {
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

const GroupedActionMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Represents a dropdown menu that contains groups of items with a header and
// optional separator per group.
export class GroupedActionMenuElement extends GroupedActionMenuElementBase {
  static get is() {
    return 'grouped-action-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      menuGroups: {type: Array},
      label: {type: String},
      nonModal: {type: Boolean},
      closeOnClick: {type: Boolean},
    };
  }

  accessor menuGroups: Array<MenuGroup<unknown>> = [];
  accessor nonModal: boolean = false;
  accessor closeOnClick: boolean = true;

  // Initializing to random value, but this is set by the parent.
  accessor label: string = '';

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    openMenu(
        this.$.lazyMenu.get(), anchor, showAtConfig, /* onShow= */ undefined,
        this.nonModal);
  }

  close() {
    this.$.lazyMenu.get().close();
  }

  protected onClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const groupIndex = Number.parseInt(currentTarget.dataset['groupIndex']!);
    const itemIndex = Number.parseInt(currentTarget.dataset['itemIndex']!);
    const group = this.menuGroups[groupIndex];
    assert(group);
    const menuItem = group.items[itemIndex];
    assert(menuItem);
    this.fire(group.eventName, {data: menuItem.data});
    if (this.closeOnClick) {
      this.$.lazyMenu.get().close();
    }
  }

  protected getAriaOwns_(groupIndex: number, length: number): string {
    return Array
        .from(
            {length}, (_, itemIndex) => `group-${groupIndex}-item-${itemIndex}`)
        .join(' ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'grouped-action-menu': GroupedActionMenuElement;
  }
}

customElements.define(GroupedActionMenuElement.is, GroupedActionMenuElement);
