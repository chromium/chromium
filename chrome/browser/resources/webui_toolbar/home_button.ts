// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './home_button.css.js';
import {getHtml} from './home_button.html.js';
import {getContextMenuPosition, getEventDispositionFlags, HelpBubbleAnchorMixin, PressHandler} from './toolbar_button.js';
import type {HomeControlState} from './toolbar_ui_api_data_model.mojom-webui.js';

const HomeButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class HomeButtonElement extends HomeButtonElementBase {
  static get is() {
    return 'home-button';
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

  accessor state: HomeControlState = {
    shouldBeShown: false,
    isContextMenuVisible: false,
  };

  protected pressHandler_: PressHandler = new PressHandler(
      this.onLongPress_.bind(this), this.onShortPress_.bind(this));

  protected getLabel_(): string {
    return loadTimeData.getString('homeButtonAccName');
  }
  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        loadTimeData.getString('homeButtonTooltip'));
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private onLongPress_(source: MenuSourceType) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kHome, getContextMenuPosition(this), source);
  }

  private onShortPress_(e: MouseEvent) {
    const flags = getEventDispositionFlags(e);
    this.browserProxy_.browserControlsHandler.navigateHome(flags);
  }

  protected onClick_(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.onShortPress_(e);
    }
  }

  protected onDragEnter_(e: DragEvent) {
    if (e.dataTransfer && e.dataTransfer.types.includes('Files') &&
        !e.dataTransfer.types.includes('text/uri-list')) {
      e.preventDefault();
    }
  }

  protected onDragOver_(e: DragEvent) {
    if (e.dataTransfer &&
        (e.dataTransfer.types.includes('text/uri-list') ||
         e.dataTransfer.types.includes('Files'))) {
      e.preventDefault();
      e.dataTransfer.dropEffect = 'copy';
    }
  }

  protected onDrop_(e: DragEvent) {
    e.preventDefault();
    e.stopPropagation();
    if (!e.dataTransfer) {
      return;
    }

    const url = e.dataTransfer.getData('text/uri-list');
    if (url) {
      this.browserProxy_.toolbarUIHandler.onHomeButtonDropUrl(url.split('\n')[0]!);
    } else if (e.dataTransfer.types.includes('Files')) {
      this.browserProxy_.toolbarUIHandler.onHomeButtonDropFile({x: e.clientX, y: e.clientY});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'home-button': HomeButtonElement;
  }
}

customElements.define(HomeButtonElement.is, HomeButtonElement);
