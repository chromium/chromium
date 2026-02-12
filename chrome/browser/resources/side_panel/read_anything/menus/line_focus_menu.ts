// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, getLineFocusValues, LineFocusMovement, LineFocusStyle, type SettingsPrefs, type ShowAtConfigPrefs, ToolbarEvent} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getHtml} from './line_focus_menu.html.js';
import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LineFocusMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LineFocusMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the color theme menu.
export class LineFocusMenuElement extends LineFocusMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'line-focus-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
      lineFocusStyle: {type: Object},
      lineFocusMovement: {type: Object},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusMovement: LineFocusMovement|null = null;

  private styleOptions_: Array<MenuStateItem<LineFocusStyle>> = [
    {
      header: {
        title: loadTimeData.getString('lineFocusStyleHeading'),
        separator: false,
      },
      title: loadTimeData.getString('lineFocusOffTitle'),
      data: LineFocusStyle.OFF,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    },
    {
      title: loadTimeData.getString('lineFocusUnderlineTitle'),
      data: LineFocusStyle.UNDERLINE,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    },
    {
      title: loadTimeData.getString('lineFocusOneLineTitle'),
      data: LineFocusStyle.SMALL_WINDOW,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    },
    {
      title: loadTimeData.getString('lineFocusThreeLineTitle'),
      data: LineFocusStyle.MEDIUM_WINDOW,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    },
    {
      title: loadTimeData.getString('lineFocusFiveLineTitle'),
      data: LineFocusStyle.LARGE_WINDOW,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    },
  ];

  private movementOptions_: Array<MenuStateItem<LineFocusMovement>> = [
    {
      header: {
        title: loadTimeData.getString('lineFocusMovementHeading'),
        separator: true,
      },
      title: loadTimeData.getString('lineFocusStaticTitle'),
      data: LineFocusMovement.STATIC,
      eventName: ToolbarEvent.LINE_FOCUS_MOVEMENT,
    },
    {
      title: loadTimeData.getString('lineFocusCursorLineTitle'),
      data: LineFocusMovement.CURSOR,
      eventName: ToolbarEvent.LINE_FOCUS_MOVEMENT,
    },
  ];
  protected options_: Array<MenuStateItem<LineFocusStyle|LineFocusMovement>> = [
    ...this.styleOptions_,
    ...this.movementOptions_,
  ];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs')) {
      this.restoreFromPrefs_();
    }

    if (changedProperties.has('lineFocusStyle') &&
        this.lineFocusStyle !== null) {
      this.updateOptionsForStyle_(this.lineFocusStyle);
    }
    if (changedProperties.has('lineFocusMovement') &&
        this.lineFocusMovement !== null) {
      this.updateOptionsForMovement_(this.lineFocusMovement);
    }
    if (changedProperties.has('lineFocusStyle') ||
        changedProperties.has('lineFocusMovement')) {
      this.options_ = [
        ...this.styleOptions_,
        ...this.movementOptions_,
      ];
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  private restoreFromPrefs_(): void {
    const lineFocusValues = getLineFocusValues();
    const lineFocus = lineFocusValues[this.settingsPrefs['lineFocus']];
    if (lineFocus) {
      this.updateOptionsForStyle_(lineFocus.style);
      this.updateOptionsForMovement_(lineFocus.movement);
      this.options_ = [
        ...this.styleOptions_,
        ...this.movementOptions_,
      ];
    }
  }

  protected onLineFocusStyleChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE);
  }

  protected onLineFocusMovementChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE);
  }

  private updateOptionsForStyle_(newStyle: LineFocusStyle) {
    this.styleOptions_.forEach(option => {
      option.selected = option.data === newStyle;
    });
  }

  private updateOptionsForMovement_(newMovement: LineFocusMovement) {
    this.movementOptions_.forEach(option => {
      option.selected = option.data === newMovement;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-focus-menu': LineFocusMenuElement;
  }
}

customElements.define(LineFocusMenuElement.is, LineFocusMenuElement);
