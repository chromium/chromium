// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Appearance menu element used for the improved Read Aloud UI.

import './grouped_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

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

  private colorOptions_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('defaultColorTitle'),
      icon: 'read-anything-20:default-theme',
      data: chrome.readingMode.defaultTheme,
    },
    {
      title: loadTimeData.getString('lightColorTitle'),
      icon: 'read-anything-20:light-theme',
      data: chrome.readingMode.lightTheme,
    },
    {
      title: loadTimeData.getString('darkColorTitle'),
      icon: 'read-anything-20:dark-theme',
      data: chrome.readingMode.darkTheme,
    },
    {
      title: loadTimeData.getString('yellowColorTitle'),
      icon: 'read-anything-20:yellow-theme',
      data: chrome.readingMode.yellowTheme,
    },
    {
      title: loadTimeData.getString('blueColorTitle'),
      icon: 'read-anything-20:blue-theme',
      data: chrome.readingMode.blueTheme,
    },
    {
      title: loadTimeData.getString('highContrastColorTitle'),
      icon: 'read-anything-20:high-contrast-theme',
      data: chrome.readingMode.highContrastTheme,
    },
    {
      title: loadTimeData.getString('lowContrastLightColorTitle'),
      icon: 'read-anything-20:low-contrast-light-theme',
      data: chrome.readingMode.lowContrastLightTheme,
    },
    {
      title: loadTimeData.getString('lowContrastDarkColorTitle'),
      icon: 'read-anything-20:low-contrast-dark-theme',
      data: chrome.readingMode.lowContrastDarkTheme,
    },
  ];

  private viewOptions_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('sidePanelLabel'),
      data: chrome.readingMode.inSidePanelPresentationState,
    },
    {
      title: loadTimeData.getString('fullPageLabel'),
      data: chrome.readingMode.inImmersiveOverlayPresentationState,
    },
  ];

  protected accessor groups_: Array<MenuGroup<number>> = [
    {
      header: {
        title: loadTimeData.getString('themeTitle'),
        separator: false,
      },
      items: this.colorOptions_,
      eventName: ToolbarEvent.THEME,
    },
    {
      header: {
        title: loadTimeData.getString('viewLabel'),
        separator: true,
      },
      items: this.viewOptions_,
      eventName: ToolbarEvent.PRESENTATION_CHANGE,
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
    chrome.readingMode.onThemeChange(newTheme);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.THEME_CHANGE);
    this.settingsPrefs = {
      ...this.settingsPrefs,
      theme: newTheme,
    };
  }

  protected onPresentationChange_(e: CustomEvent<{data: number}>) {
    const newPresentationState = e.detail.data;
    if (newPresentationState !== this.presentationState) {
      chrome.readingMode.togglePresentation();
      this.presentationState = newPresentationState;
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
