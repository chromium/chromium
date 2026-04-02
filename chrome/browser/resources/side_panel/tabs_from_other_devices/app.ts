// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSessionPageHandlerRemote} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {ForeignSessionPageCallbackRouter, ForeignSessionPageHandler} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

const TabsFromOtherDevicesAppElementBase = CrLitElement;

export class TabsFromOtherDevicesAppElement extends
    TabsFromOtherDevicesAppElementBase {
  static get is() {
    return 'tabs-from-other-devices-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  private foreignSessionHandler: ForeignSessionPageHandlerRemote =
      ForeignSessionPageHandler.getRemote();
  private foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter =
      new ForeignSessionPageCallbackRouter();

  constructor() {
    super();
    this.foreignSessionHandler.setPage(
        this.foreignSessionCallbackRouter.$.bindNewPipeAndPassRemote());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabs-from-other-devices-app': TabsFromOtherDevicesAppElement;
  }
}

customElements.define(
    TabsFromOtherDevicesAppElement.is, TabsFromOtherDevicesAppElement);
