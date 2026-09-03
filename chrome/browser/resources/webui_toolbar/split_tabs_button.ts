// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './icons.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {ContextMenuType, SplitTabActiveLocation} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {SplitTabsControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl, INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {OverflowableButtonMixin} from './overflowable_button.js';
import {getHtml} from './split_tabs_button.html.js';
import {getCss} from './toolbar_button.css.js';
import {BUTTON_LEFT, getClickSourceType, getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin, roundedIconsEnabled} from './toolbar_button.js';

const SplitTabsButtonElementBase =
    HelpBubbleAnchorMixin(OverflowableButtonMixin(CrLitElement));

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

  override accessor state: SplitTabsControlState = {
    isCurrentTabSplit: false,
    location: SplitTabActiveLocation.kStart,
    shouldBeShown: false,
    isContextMenuVisible: false,
    menuOpenToken: 0,
  };
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private showMenuListenerHandle_: number =
      INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE;

  override connectedCallback() {
    super.connectedCallback();
    // Call `showContextMenuAndPreventOverflow()` directly instead of showing
    // the context menu directly, just in case WebUI page tab hasn't yet been
    // informed been informed that the tab has been split. There's still the
    // opposite issue, where the tab was unsplit before this message reached the
    // renderer, but since the browser process checks split tab state
    // immediately before sending this message, it's unlikely that the WebUI
    // renderer process will have more up-to-date state information that the
    // browser process code that decided to invoke this method to show a menu
    // rather than to split the tab.
    this.showMenuListenerHandle_ =
        this.browserProxy_.addShowSplitTabsContextMenuListener(
            () => this.showContextMenuAndPreventOverflow(
                ContextMenuType.kSplitTabsAction, MenuSourceType.kNone));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.removeShowSplitTabsContextMenuListener(
        this.showMenuListenerHandle_);
  }

  protected getIcon(): string {
    let iconName = 'split_scene';
    if (this.state.isCurrentTabSplit) {
      switch (this.state.location) {
        case SplitTabActiveLocation.kStart:
          iconName = 'split_scene_left';
          break;
        case SplitTabActiveLocation.kEnd:
          iconName = 'split_scene_right';
          break;
        case SplitTabActiveLocation.kTop:
          iconName = 'split_scene_up';
          break;
        case SplitTabActiveLocation.kBottom:
          iconName = 'split_scene_down';
          break;
        default:
          break;
      }
    }
    if (!roundedIconsEnabled()) {
      iconName += '_old';
    }
    return `webui-toolbar:${iconName}`;
  }

  protected getLabel(): string {
    if (!this.state.isCurrentTabSplit) {
      return loadTimeData.getString('splitTabsButtonAccNamePinned');
    }
    const isRtl = loadTimeData.getString('textdirection') === 'rtl';
    let labelId = '';
    switch (this.state.location) {
      case SplitTabActiveLocation.kStart:
        labelId = isRtl ? 'splitTabsButtonAccNameEnabledRight' :
                          'splitTabsButtonAccNameEnabledLeft';
        break;
      case SplitTabActiveLocation.kEnd:
        labelId = isRtl ? 'splitTabsButtonAccNameEnabledLeft' :
                          'splitTabsButtonAccNameEnabledRight';
        break;
      case SplitTabActiveLocation.kTop:
        labelId = 'splitTabsButtonAccNameEnabledTop';
        break;
      case SplitTabActiveLocation.kBottom:
        labelId = 'splitTabsButtonAccNameEnabledBottom';
        break;
      default:
        labelId = 'splitTabsButtonAccNamePinned';
        break;
    }
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
      // If already split, show the action menu. Use
      // showContextMenuAndPreventOverflow() so that button becomes visible if
      // it has currently overflowed, as would be the case when invoked from a
      // showSplitTabsContextMenu() call from the browser process.
      this.showContextMenuAndPreventOverflow(
          ContextMenuType.kSplitTabsAction, sourceType);
    } else {
      // If not split, enters split view.
      this.browserProxy_.browserControlsHandler.splitActiveTab();
    }
  }

  protected onContextmenu(e: PointerEvent) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kSplitTabsContext, getContextMenuPosition(this),
        getContextMenuSourceType(e), /*showMenuToken=*/ null);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'split-tabs-button': SplitTabsButtonElement;
  }
}

customElements.define(SplitTabsButtonElement.is, SplitTabsButtonElement);
