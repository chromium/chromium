// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grouped_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, LineFocusMovement, LineFocusStyle, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {GroupedActionMenuElement} from './grouped_action_menu.js';
import {getHtml} from './line_focus_menu.html.js';
import type {MenuGroup, MenuStateItem, ToolbarMenu} from './menu_util.js';

export interface LineFocusMenuElement {
  $: {
    menu: GroupedActionMenuElement,
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
      lineFocusMovement: {type: Number},
      groups_: {type: Array},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusMovement: LineFocusMovement|null = null;

  private styleOptions_: Array<MenuStateItem<LineFocusStyle>> = [
    {
      title: loadTimeData.getString('lineFocusOffTitle'),
      data: LineFocusStyle.OFF,
    },
    {
      title: loadTimeData.getString('lineFocusUnderlineTitle'),
      data: LineFocusStyle.UNDERLINE,
    },
    {
      title: loadTimeData.getString('lineFocusOneLineTitle'),
      data: LineFocusStyle.SMALL_WINDOW,
    },
    {
      title: loadTimeData.getString('lineFocusThreeLineTitle'),
      data: LineFocusStyle.MEDIUM_WINDOW,
    },
    {
      title: loadTimeData.getString('lineFocusFiveLineTitle'),
      data: LineFocusStyle.LARGE_WINDOW,
    },
  ];

  private movementOptions_: Array<MenuStateItem<LineFocusMovement>> = [
    {
      title: loadTimeData.getString('lineFocusStaticTitle'),
      data: LineFocusMovement.STATIC,
    },
    {
      title: loadTimeData.getString('lineFocusCursorLineTitle'),
      data: LineFocusMovement.CURSOR,
    },
  ];

  protected accessor groups_:
      Array<MenuGroup<LineFocusStyle|LineFocusMovement>> = [
        {
          header: {
            title: loadTimeData.getString('lineFocusStyleHeading'),
            separator: false,
          },
          items: this.styleOptions_,
          eventName: ToolbarEvent.LINE_FOCUS_STYLE,
        },
        {
          header: {
            title: loadTimeData.getString('lineFocusMovementHeading'),
            separator: true,
          },
          items: this.movementOptions_,
          eventName: ToolbarEvent.LINE_FOCUS_MOVEMENT,
        },
      ];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

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
      this.groups_ = [...this.groups_];
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
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
