// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {VisualBrowserProxy} from '../app/visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';
import {ToolbarEvent} from '../content/read_anything_types.js';
import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';

import {getHtml} from './line_spacing_menu.html.js';
import {getIndexOfSetting} from './menu_util.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LineSpacingMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LineSpacingMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the line spacing menu.
export class LineSpacingMenuElement extends LineSpacingMenuElementBase
    implements ToolbarMenu {
  static get is() {
    return 'line-spacing-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      lineSpacing: {type: Number},
      nonModal: {type: Boolean},
      options_: {type: Array},
    };
  }

  accessor lineSpacing: number = 0;
  accessor nonModal: boolean = false;

  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  protected accessor options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('lineSpacingStandardTitle'),
      icon: 'read-anything:line-spacing-standard-custom',
      data: this.visualBrowserProxy_.getStandardLineSpacing(),
    },
    {
      title: loadTimeData.getString('lineSpacingLooseTitle'),
      icon: 'read-anything:line-spacing-loose-custom',
      data: this.visualBrowserProxy_.getLooseLineSpacing(),
    },
    {
      title: loadTimeData.getString('lineSpacingVeryLooseTitle'),
      icon: 'read-anything:line-spacing-very-loose-custom',
      data: this.visualBrowserProxy_.getVeryLooseLineSpacing(),
    },
  ];

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected restoredLineSpacingIndex_(): number {
    return getIndexOfSetting(this.options_, this.lineSpacing);
  }

  protected onLineSpacingChange_() {
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-spacing-menu': LineSpacingMenuElement;
  }
}

customElements.define(LineSpacingMenuElement.is, LineSpacingMenuElement);
