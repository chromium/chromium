// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrefs} from '../content/read_anything_types.js';
import {LineFocusType} from '../content/read_anything_types.js';
import type {LineFocus} from '../content/read_anything_types.js';

import {getHtml} from './line_focus_menu.html.js';
import type {MenuStateItem} from './menu_util.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface LineFocusMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const LineFocusMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the color theme menu.
export class LineFocusMenuElement extends LineFocusMenuElementBase {
  static get is() {
    return 'line-focus-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {settingsPrefs: {type: Object}};
  }

  accessor settingsPrefs: SettingsPrefs = {
    letterSpacing: 0,
    lineSpacing: 0,
    theme: 0,
    speechRate: 0,
    font: '',
    highlightGranularity: 0,
  };

  // TODO(crbug.com/467778823): Internationalize these strings once finalized.
  protected options_: Array<MenuStateItem<LineFocus>> = [
    {
      title: 'Off',
      data: {type: LineFocusType.NONE, lines: 0},
    },
    {
      title: 'Highlight 1 line',
      data: {type: LineFocusType.WINDOW, lines: 1},
    },
    {
      title: 'Highlight 3 lines',
      data: {type: LineFocusType.WINDOW, lines: 3},
    },
    {
      title: 'Highlight 5 lines',
      data: {type: LineFocusType.WINDOW, lines: 5},
    },
    {
      title: 'Line',
      data: {type: LineFocusType.LINE, lines: 1},
    },
  ];

  open(anchor: HTMLElement) {
    this.$.menu.open(anchor);
  }

  protected restoredLineFocusIndex_(): number {
    // TODO(crbug.com/447427066): Retrieve this value from prefs.
    return 0;
  }

  protected onLineFocusChange_(_event: CustomEvent<{data: number}>) {
    // TODO(crbug.com/447427066): Implement this to log the change and store
    // it in prefs.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'line-focus-menu': LineFocusMenuElement;
  }
}

customElements.define(LineFocusMenuElement.is, LineFocusMenuElement);
