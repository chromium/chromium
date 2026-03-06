// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';

import {getCss} from './back_forward_button.css.js';
import {getHtml} from './back_forward_button.html.js';
import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy, ButtonState} from './browser_proxy.js';
import {BUTTON_LEFT, BUTTON_RIGHT, getClickDispositionFlags, getContextMenuPosition} from './toolbar_button.js';

// This follows what is done in the views code.
// https://crsrc.org/c/chrome/browser/ui/views/toolbar/toolbar_button.cc;l=440
const LONG_PRESS_TIMER_THRESHOLD_MS = 500;

export class BackForwardButtonElement extends CrLitElement {
  static get is() {
    return 'back-forward-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      direction: {type: String},
      state: {type: Object},
      leadingMargin: {type: Number},
    };
  }

  accessor direction: 'back'|'forward' = 'back';
  accessor state: ButtonState = {enabled: false, visible: true};
  accessor leadingMargin: number = 0;

  private isLongPressed_: boolean = false;
  private longPressTimer_: number = 0;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      this.hidden = !this.state.visible;
    }
  }

  private get contextMenuType_(): ContextMenuType {
    return this.direction === 'back' ? ContextMenuType.kBack :
                                       ContextMenuType.kForward;
  }

  private openMenu_(source: MenuSourceType) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        this.contextMenuType_, getContextMenuPosition(this), source);
  }

  protected get ariaLabel_(): string {
    return this.direction === 'back' ?
        loadTimeData.getString('backButtonAccName') :
        loadTimeData.getString('forwardButtonAccName');
  }

  protected get tooltip_(): string {
    return this.direction === 'back' ?
        loadTimeData.getString('backButtonTooltip') :
        loadTimeData.getString('forwardButtonTooltip');
  }

  protected onContextmenu_(e: MouseEvent) {
    // TODO(crbug.com/470038385): Handle the event source type (such as touch
    // event) like what is done for split tabs button.
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        this.contextMenuType_, getContextMenuPosition(this),
        MenuSourceType.kMouse);
    e.preventDefault();
  }

  protected onPointerdown_(e: MouseEvent) {
    // Mac Ctrl+click: Open menu immediately and BAIL.
    if (isMac && e.button === BUTTON_LEFT && e.ctrlKey) {
      this.openMenu_(MenuSourceType.kMouse);
      return;
    }

    if (e.button === BUTTON_RIGHT) {
      return;
    }

    this.isLongPressed_ = false;
    clearTimeout(this.longPressTimer_);

    this.longPressTimer_ = setTimeout(() => {
      this.isLongPressed_ = true;
      this.openMenu_(MenuSourceType.kLongPress);
    }, LONG_PRESS_TIMER_THRESHOLD_MS);
  }

  protected onPointerup_(e: MouseEvent) {
    const isMacCtrlClick = isMac && e.button === BUTTON_LEFT && e.ctrlKey;

    // Block navigation if:
    // 1. It was a long press.
    // 2. It's a Mac Ctrl+Click
    if (this.isLongPressed_ || isMacCtrlClick) {
      this.isLongPressed_ = false;
      clearTimeout(this.longPressTimer_);
      return;
    }

    clearTimeout(this.longPressTimer_);

    const flags = getClickDispositionFlags(e);

    if (this.direction === 'back') {
      this.browserProxy_.browserControlsHandler.back(flags);
    } else {
      this.browserProxy_.browserControlsHandler.forward(flags);
    }
  }

  protected onPointercancel_() {
    clearTimeout(this.longPressTimer_);
  }

  protected onPointerenter_() {
    if (this.direction === 'back') {
      this.browserProxy_.browserControlsHandler.backButtonHovered();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'back-forward-button': BackForwardButtonElement;
  }
}

customElements.define(BackForwardButtonElement.is, BackForwardButtonElement);
