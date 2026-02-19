// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './split_tabs_button_icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {ContextMenuType, SplitTabActiveLocation} from './browser_controls_api_data_model.mojom-webui.js';
import type {SplitTabsControlState} from './browser_controls_api_data_model.mojom-webui.js';
import {type BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './split_tabs_button.css.js';
import {getHtml} from './split_tabs_button.html.js';
import {getContextMenuPosition} from './toolbar_button.js';

export class SplitTabsButtonElement extends CrLitElement {
  static get is() {
    return 'split-tabs-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
    };
  }

  protected accessor state: SplitTabsControlState = {
    isCurrentTabSplit: false,
    location: SplitTabActiveLocation.kStart,
    isPinned: false,
    isContextMenuVisible: false,
  };
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('state')) {
      this.hidden = !this.state.isPinned && !this.state.isCurrentTabSplit;
    }
  }

  protected getIcon(): string {
    let iconName = 'split-scene';
    if (this.state.isCurrentTabSplit) {
      switch (this.state.location) {
        case SplitTabActiveLocation.kStart:
          iconName = 'split-scene-left';
          break;
        case SplitTabActiveLocation.kEnd:
          iconName = 'split-scene-right';
          break;
        case SplitTabActiveLocation.kTop:
          iconName = 'split-scene-up';
          break;
        case SplitTabActiveLocation.kBottom:
          iconName = 'split-scene-down';
          break;
        default:
          break;
      }
    }
    return `split-tabs-button:${iconName}`;
  }

  protected getLabel(): string {
    const labelId = this.state.isCurrentTabSplit ?
        'splitTabsButtonAccNameEnabled' :
        'splitTabsButtonAccNamePinned';
    return loadTimeData.getString(labelId);
  }

  protected onClick() {
    if (this.state.isCurrentTabSplit) {
      // If already split, show the action menu.
      this.browserProxy_.handler.showContextMenu(
          ContextMenuType.kSplitTabsAction, this.menuPosition(),
          MenuSourceType.kMouse);
    } else {
      // If not split, enters split view.
      this.browserProxy_.handler.splitActiveTab();
    }
  }

  protected onContextMenu(e: MouseEvent) {
    e.preventDefault();
    this.browserProxy_.handler.showContextMenu(
        ContextMenuType.kSplitTabsContext, this.menuPosition(),
        MenuSourceType.kMouse);
  }

  protected menuPosition() {
    return getContextMenuPosition(this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'split-tabs-button-app': SplitTabsButtonElement;
  }
}

customElements.define(SplitTabsButtonElement.is, SplitTabsButtonElement);
