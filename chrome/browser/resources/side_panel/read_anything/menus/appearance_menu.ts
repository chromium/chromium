// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Appearance menu element used for the improved Read Aloud UI.

import './grouped_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {VisualBrowserProxy} from '../app/visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';
import {DEFAULT_SETTINGS, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getHtml} from './appearance_menu.html.js';
import type {GroupedActionMenuElement} from './grouped_action_menu.js';
import type {MenuGroup, MenuStateItem, ToolbarMenu} from './menu_util.js';

export interface AppearanceMenuElement {
  $: {
    menu: GroupedActionMenuElement,
  };
}

const AppearanceMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the appearance menu.
export class AppearanceMenuElement extends AppearanceMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'appearance-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
      presentationState: {type: Number},
      groups_: {type: Array},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor presentationState: number = 0;

  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  private colorOptions_: Array<MenuStateItem<number>> = [
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

  private viewOptions_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('sidePanelLabel'),
      data: this.visualBrowserProxy_.getInSidePanelPresentationState(),
    },
    {
      title: loadTimeData.getString('fullPageLabel'),
      data: this.visualBrowserProxy_.getInImmersiveOverlayPresentationState(),
    },
  ];

  protected accessor groups_: Array<MenuGroup<number>> = [
     {
      header: {
        title: loadTimeData.getString('viewLabel'),
        separator: false,
      },
      items: this.viewOptions_,
      eventName: ToolbarEvent.PRESENTATION_CHANGE,
    },
    {
      header: {
        title: loadTimeData.getString('themeTitle'),
        separator: true,
      },
      items: this.colorOptions_,
      eventName: ToolbarEvent.THEME,
    },
  ];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs')) {
      this.updateOptionsForTheme_();
    }
    if (changedProperties.has('presentationState')) {
      this.updateOptionsForPresentation_();
    }
    if (changedProperties.has('settingsPrefs') ||
        changedProperties.has('presentationState')) {
      this.groups_ = [...this.groups_];
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected onThemeChange_(e: CustomEvent<{data: number}>) {
    const newTheme = e.detail.data;
    this.visualBrowserProxy_.onThemeChange(newTheme);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.THEME_CHANGE);
    this.settingsPrefs = {
      ...this.settingsPrefs,
      theme: newTheme,
    };
  }

  protected onPresentationChange_(e: CustomEvent<{data: number}>) {
    const newPresentationState = e.detail.data;
    if (newPresentationState !== this.presentationState) {
      this.visualBrowserProxy_.togglePresentation();
    }
  }

  private updateOptionsForTheme_() {
    const currentTheme = this.settingsPrefs.theme;
    this.colorOptions_.forEach(option => {
      option.selected = option.data === currentTheme;
    });
  }

  private updateOptionsForPresentation_() {
    this.viewOptions_.forEach(option => {
      option.selected = option.data === this.presentationState;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'appearance-menu': AppearanceMenuElement;
  }
}

customElements.define(AppearanceMenuElement.is, AppearanceMenuElement);
