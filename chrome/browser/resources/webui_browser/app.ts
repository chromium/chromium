// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './webview.js';
import '//resources/cr_components/searchbox/searchbox.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export class WebuiBrowserAppElement extends CrLitElement {
  static get is() {
    return 'webui-browser-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
      guestId_: {type: Number},
    };
  }

  protected accessor message_: string = loadTimeData.getString('message');
  protected accessor guestId_: number = loadTimeData.getInteger('testGuestId');

  protected onLaunchDevtoolsClick_(_: Event) {
    BrowserProxy.getPageHandler().launchDevToolsForBrowser();
  }

  protected onAppMenuClick_(_: Event) {
    BrowserProxy.getPageHandler().openAppMenu();
  }

  protected onMinimizeClick_(_: Event) {
    BrowserProxy.getPageHandler().minimize();
  }

  protected onMaximizeClick_(_: Event) {
    BrowserProxy.getPageHandler().maximize();
  }

  protected onRestoreClick_(_: Event) {
    BrowserProxy.getPageHandler().restore();
  }

  protected onCloseClick_(_: Event) {
    BrowserProxy.getPageHandler().close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-app': WebuiBrowserAppElement;
  }
}

customElements.define(WebuiBrowserAppElement.is, WebuiBrowserAppElement);
