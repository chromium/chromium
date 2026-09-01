// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './icons.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './battery_saver_button.html.js';
import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './toolbar_button.css.js';
import {getContextMenuPosition, getContextMenuSourceType} from './toolbar_button.js';

export interface BatterySaverButtonElement {
  $: {
    button: CrIconButtonElement,
  };
}

export class BatterySaverButtonElement extends CrLitElement {
  static get is() {
    return 'battery-saver-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected getLabel_(): string {
    return loadTimeData.getString('batterySaverButtonAccName');
  }

  protected getTooltip_(): string {
    return loadTimeData.getString('batterySaverButtonTooltip');
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected onClick_(e: MouseEvent) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kBatterySaver, getContextMenuPosition(this),
        getContextMenuSourceType(e));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'battery-saver-button': BatterySaverButtonElement;
  }
}

customElements.define(BatterySaverButtonElement.is, BatterySaverButtonElement);
