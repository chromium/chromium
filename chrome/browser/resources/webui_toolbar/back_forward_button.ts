// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {getCss} from './back_forward_button.css.js';
import {getHtml} from './back_forward_button.html.js';
import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy, ButtonState} from './browser_proxy.js';
import {getClickDispositionFlags, getContextMenuPosition, PressHandler} from './toolbar_button.js';

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

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected pressHandler_: PressHandler = new PressHandler(
      this.onLongPress_.bind(this), this.onShortPress_.bind(this));

  private get contextMenuType_(): ContextMenuType {
    return this.direction === 'back' ? ContextMenuType.kBack :
                                       ContextMenuType.kForward;
  }

  private onLongPress_(source: MenuSourceType) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        this.contextMenuType_, getContextMenuPosition(this), source);
  }

  private onShortPress_(e: PointerEvent) {
    const flags = getClickDispositionFlags(e);

    if (this.direction === 'back') {
      this.browserProxy_.browserControlsHandler.back(flags);
    } else {
      this.browserProxy_.browserControlsHandler.forward(flags);
    }
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
