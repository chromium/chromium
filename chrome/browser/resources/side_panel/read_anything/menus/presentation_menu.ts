// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './simple_action_menu.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../content/read_anything_types.js';
import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';

import type {MenuStateItem, ToolbarMenu} from './menu_util.js';
import {getIndexOfSetting} from './menu_util.js';
import {getHtml} from './presentation_menu.html.js';
import type {SimpleActionMenuElement} from './simple_action_menu.js';

export interface PresentationMenuElement {
  $: {
    menu: SimpleActionMenuElement,
  };
}

const PresentationMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the view menu.
export class PresentationMenuElement extends PresentationMenuElementBase
    implements ToolbarMenu {
  static get is() {
    return 'presentation-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      presentationState: {type: Number},
      nonModal: {type: Boolean},
    };
  }
  accessor presentationState: number = 0;
  accessor nonModal: boolean = true;

  protected options_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('sidePanelLabel'),
      data: chrome.readingMode.inSidePanelPresentationState,
    },
    {
      title: loadTimeData.getString('fullPageLabel'),
      data: chrome.readingMode.inImmersiveOverlayPresentationState,
    },
  ];

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
  }

  protected restoredPresentationIndex_(): number {
    return getIndexOfSetting(this.options_, this.presentationState);
  }

  protected onPresentationChange_(e: CustomEvent<{data: number}>) {
    if (e.detail.data !== this.presentationState) {
      chrome.readingMode.togglePresentation();
    }
    this.fire(ToolbarEvent.CLOSE_ALL_MENUS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'presentation-menu': PresentationMenuElement;
  }
}

customElements.define(PresentationMenuElement.is, PresentationMenuElement);
