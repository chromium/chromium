// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './icons.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {MediaControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getHtml} from './media_button.html.js';
import {getCss} from './toolbar_button.css.js';
import {getClickSourceType, getContextMenuPosition, HelpBubbleAnchorMixin, PressHandler} from './toolbar_button.js';

const MediaButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class MediaButtonElement extends MediaButtonElementBase {
  static get is() {
    return 'media-button';
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

  accessor state: MediaControlState = {
    enabled: true,
    shouldBeShown: false,
    isContextMenuVisible: false,
  };

  protected pressHandler_: PressHandler = new PressHandler(
      this.onLongPress_.bind(this), this.onShortPress_.bind(this));

  protected getTooltip_(): string {
    return loadTimeData.getString('mediaButtonTooltip');
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private onLongPress_(source: MenuSourceType) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kMedia, getContextMenuPosition(this), source,
        /*showMenuToken=*/ null);
  }

  private onShortPress_(e: MouseEvent) {
    this.browserProxy_.toolbarUIHandler.onMediaButtonClicked(
        getClickSourceType(e) === MenuSourceType.kMouse);
  }

  protected onClick_(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.onShortPress_(e);
    }
  }

  protected onPointerdown_(e: PointerEvent) {
    if (e.button === 0) {
      this.browserProxy_.toolbarUIHandler.onMediaButtonMousePressed();
    }
    this.pressHandler_.onPointerdown(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'media-button': MediaButtonElement;
  }
}

customElements.define(MediaButtonElement.is, MediaButtonElement);
