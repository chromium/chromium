// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ForeignSession, ForeignSessionPageHandlerRemote} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {ForeignSessionPageCallbackRouter, ForeignSessionPageHandler} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

const TabsFromOtherDevicesAppElementBase = CrLitElement;

export class TabsFromOtherDevicesAppElement extends
    TabsFromOtherDevicesAppElementBase {
  static get is() {
    return 'tabs-from-other-devices-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      syncedDevices_: {type: Array},
      loading_: {type: Boolean},
      selectedDeviceTag_: {type: String},
    };
  }

  protected accessor syncedDevices_: ForeignSession[] = [];
  protected accessor loading_: boolean = true;
  protected accessor selectedDeviceTag_: string|null = null;

  private foreignSessionHandler: ForeignSessionPageHandlerRemote =
      ForeignSessionPageHandler.getRemote();
  private foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter =
      new ForeignSessionPageCallbackRouter();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().refreshColorsCss();

    this.foreignSessionCallbackRouter.onForeignSessionsChanged.addListener(
        (sessionList: ForeignSession[]) =>
            this.setForeignSessions_(sessionList));

    this.foreignSessionHandler.getForeignSessions().then(
        (res: {sessions: ForeignSession[]}) => {
          this.setForeignSessions_(res.sessions);
          this.foreignSessionHandler.setPage(
              this.foreignSessionCallbackRouter.$.bindNewPipeAndPassRemote());
        });
  }

  private setForeignSessions_(sessions: ForeignSession[]) {
    this.syncedDevices_ = sessions;
    this.loading_ = false;
    if (!this.selectedDeviceTag_ && sessions.length > 0) {
      this.selectedDeviceTag_ = sessions[0]!.tag;
    }
  }

  protected onDeviceSelectClick_(e: MouseEvent) {
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#deviceMenu')!;
    menu.showAt(e.target as HTMLElement);
  }

  protected onDeviceItemClick_(e: MouseEvent) {
    const target = e.currentTarget as HTMLElement;
    this.selectedDeviceTag_ = target.dataset['tag']!;
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#deviceMenu')!;
    menu.close();
  }

  protected getSelectedDeviceName_(): string {
    const device =
        this.syncedDevices_.find(d => d.tag === this.selectedDeviceTag_);
    return device ? device.name : '';
  }

  protected onTabAuxclick_(e: MouseEvent) {
    this.onTabClick_(e);
  }

  protected onTabClick_(e: MouseEvent) {
    if (e.button > 1) {
      return;
    }

    const target = e.currentTarget as HTMLElement;
    const sessionTag = target.dataset['sessionTag']!;
    const tabId = parseInt(target.dataset['tabId']!, 10);

    const modifiers: ClickModifiers = {
      middleButton: e.button === 1,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    };
    this.foreignSessionHandler.openForeignSessionTab(
        sessionTag, tabId, modifiers);
  }

  protected getHostname_(urlStr: string): string {
    try {
      return new URL(urlStr).hostname;
    } catch (e) {
      return urlStr;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabs-from-other-devices-app': TabsFromOtherDevicesAppElement;
  }
}

customElements.define(
    TabsFromOtherDevicesAppElement.is, TabsFromOtherDevicesAppElement);
