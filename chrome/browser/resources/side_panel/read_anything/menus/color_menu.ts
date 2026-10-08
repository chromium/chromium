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

import {getHtml} from './color_menu.html.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface ColorMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const ColorMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the color theme menu.
export class ColorMenuElement extends ColorMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'color-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme: {type: Number},
      nonModal: {type: Boolean},
      options_: {type: Array},
    };
  }

  accessor theme: number = 0;
  accessor nonModal: boolean = false;

  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  protected accessor options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('defaultColorTitle'),
      icon: 'read-anything-20:default-theme-custom',
      data: this.visualBrowserProxy_.getDefaultTheme(),
    },
    {
      title: loadTimeData.getString('lightColorTitle'),
      icon: 'read-anything-20:light-theme-custom',
      data: this.visualBrowserProxy_.getLightTheme(),
    },
    {
      title: loadTimeData.getString('darkColorTitle'),
      icon: 'read-anything-20:dark-theme-custom',
      data: this.visualBrowserProxy_.getDarkTheme(),
    },
    {
      title: loadTimeData.getString('yellowColorTitle'),
      icon: 'read-anything-20:yellow-theme-custom',
      data: this.visualBrowserProxy_.getYellowTheme(),
    },
    {
      title: loadTimeData.getString('blueColorTitle'),
      icon: 'read-anything-20:blue-theme-custom',
      data: this.visualBrowserProxy_.getBlueTheme(),
    },
    {
      title: loadTimeData.getString('highContrastColorTitle'),
      icon: 'read-anything-20:high-contrast-theme-custom',
      data: this.visualBrowserProxy_.getHighContrastTheme(),
    },
    {
      title: loadTimeData.getString('lowContrastLightColorTitle'),
      icon: 'read-anything-20:low-contrast-light-theme-custom',
      data: this.visualBrowserProxy_.getLowContrastLightTheme(),
    },
    {
      title: loadTimeData.getString('lowContrastDarkColorTitle'),
      icon: 'read-anything-20:low-contrast-dark-theme-custom',
      data: this.visualBrowserProxy_.getLowContrastDarkTheme(),
    },
  ];

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected restoredThemeIndex_(): number {
    return getIndexOfSetting(this.options_, this.theme);
  }

  protected onThemeChange_() {
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'color-menu': ColorMenuElement;
  }
}

customElements.define(ColorMenuElement.is, ColorMenuElement);
