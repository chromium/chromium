// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grouped_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {browserProxyFactory as userEducationProxyFactory} from '//resources/mojo/components/user_education/webui/user_education.mojom-webui.js';

import type {VisualBrowserProxy} from '../app/visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';
import {DEFAULT_SETTINGS, LineFocusMovement, LineFocusStyle, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {GroupedActionMenuElement} from './grouped_action_menu.js';
import {getHtml} from './line_focus_menu.html.js';
import type {MenuGroup, MenuStateItem, ToolbarMenu} from './menu_util.js';

export const LINE_FOCUS_FEATURE_NAME = 'ReadAnythingLineFocus';

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
      lineFocusEnabled: {type: Boolean},
      lineFocusMovement: {type: Number},
      groups_: {type: Array},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusEnabled: boolean = false;
  accessor lineFocusMovement: LineFocusMovement|null = null;

  proxy: Object|undefined;

  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  private toggleOptions_: Array<MenuStateItem<boolean>> = [
    {
      title: loadTimeData.getString('lineFocusOffTitle'),
      data: false,
    },
    {
      title: loadTimeData.getString('lineFocusOnTitle'),
      data: true,
    },
  ];

  private styleOptions_: Array<MenuStateItem<LineFocusStyle>> = [
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

  private get toggleGroup_(): MenuGroup<boolean> {
    return {
      header: {
        title: loadTimeData.getString('lineFocusLabel'),
        shortcut: loadTimeData.getString('lineFocusShortcutLabel'),
        separator: false,
      },
      items: this.toggleOptions_,
      eventName: ToolbarEvent.LINE_FOCUS_TOGGLE,
    };
  }

  private get styleGroup_(): MenuGroup<LineFocusStyle> {
    return {
      header: {
        title: loadTimeData.getString('lineFocusStyleHeading'),
        separator: true,
      },
      items: this.styleOptions_,
      eventName: ToolbarEvent.LINE_FOCUS_STYLE,
    };
  }

  private get movementGroup_(): MenuGroup<LineFocusMovement> {
    return {
      header: {
        title: loadTimeData.getString('lineFocusMovementHeading'),
        separator: true,
      },
      items: this.movementOptions_,
      eventName: ToolbarEvent.LINE_FOCUS_MOVEMENT,
    };
  }

  protected accessor groups_:
      Array<MenuGroup<LineFocusStyle|LineFocusMovement|boolean>> = [];
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('lineFocusEnabled')) {
      this.updateOptionsForToggle_(this.lineFocusEnabled);
    }
    if (changedProperties.has('lineFocusStyle') &&
        this.lineFocusStyle !== null) {
      this.updateOptionsForStyle_(this.lineFocusStyle);
    }
    if (changedProperties.has('lineFocusMovement') &&
        this.lineFocusMovement !== null) {
      this.updateOptionsForMovement_(this.lineFocusMovement);
    }
    if (changedProperties.has('lineFocusEnabled') ||
        changedProperties.has('lineFocusStyle') ||
        changedProperties.has('lineFocusMovement')) {
      this.computeGroups_();
    }
  }

  private computeGroups_() {
    const groups: Array<MenuGroup<LineFocusStyle|LineFocusMovement|boolean>> = [
      this.toggleGroup_,
    ];

    if (!this.visualBrowserProxy_.isReadAnythingImprovedUiEnabled() ||
        this.lineFocusEnabled) {
      groups.push(this.styleGroup_, this.movementGroup_);
    }

    this.groups_ = groups;
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
    if (this.lineFocusEnabled) {
      this.proxy = userEducationProxyFactory;
      userEducationProxyFactory.getInstance().handler.notifyNewBadgeFeatureUsed(
          LINE_FOCUS_FEATURE_NAME);
    }
  }

  protected onLineFocusStyleChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE);
  }

  protected onLineFocusToggleChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_TOGGLE);
  }

  protected onLineFocusMovementChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE);
  }

  private updateOptionsForToggle_(isEnabled: boolean) {
    this.toggleOptions_.forEach(option => {
      option.selected = option.data === isEnabled;
    });
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
