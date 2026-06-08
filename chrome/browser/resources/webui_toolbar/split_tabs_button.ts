// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './split_tabs_button_icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './split_tabs_button.css.js';
import {getHtml} from './split_tabs_button.html.js';
import {BUTTON_LEFT, getClickSourceType, getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin} from './toolbar_button.js';
import {ContextMenuType, SplitTabActiveLocation} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {SplitTabsControlState} from './toolbar_ui_api_data_model.mojom-webui.js';

const SplitTabsButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class SplitTabsButtonElement extends SplitTabsButtonElementBase {
  static get is() {
    return 'split-tabs-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
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

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.getLabel());
  }

  /**
   * Pointer and mouse clicks should be handled by onPointerdown to trigger
   * immediate response to match native Views behavior.
   * @param e the PointerEvent associated with the click.
   * @returns
   */
  protected onPointerdown(e: PointerEvent) {
    // Only handle primary (left) clicks. Right clicks are handled by the
    // @contextmenu listener. Split tabs button does not support middle click.
    if (e.button !== BUTTON_LEFT) {
      return;
    }
    this.handleAction_(getClickSourceType(e));
  }

  protected onClick(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.handleAction_(getClickSourceType(e));
    }
  }

  private handleAction_(sourceType: MenuSourceType) {
    if (this.state.isCurrentTabSplit) {
      // If already split, show the action menu.
      this.browserProxy_.toolbarUIHandler.showContextMenu(
          ContextMenuType.kSplitTabsAction, getContextMenuPosition(this),
          sourceType);
    } else {
      // If not split, enters split view.
      this.browserProxy_.browserControlsHandler.splitActiveTab();
    }
  }

  protected onContextmenu(e: PointerEvent) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kSplitTabsContext, getContextMenuPosition(this),
        getContextMenuSourceType(e));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'split-tabs-button': SplitTabsButtonElement;
  }
}

customElements.define(SplitTabsButtonElement.is, SplitTabsButtonElement);
