// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './http_message_object.js';
import './shared_style.css.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './http_tab.html.js';
import {NearbyHttpBrowserProxy} from './nearby_http_browser_proxy.js';
import type {HttpMessage} from './types.js';

const HttpTabElementBase = WebUiListenerMixin(PolymerElement);

class HttpTabElement extends HttpTabElementBase {
  static get is() {
    return 'http-tab';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

      httpMessageList_: {
        type: Array,
        value: () => [],
      },

    };
  }

  private browserProxy_: NearbyHttpBrowserProxy =
      NearbyHttpBrowserProxy.getInstance();
  private httpMessageList_: HttpMessage[];

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript and
   * initialize WebUI Listeners.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'http-message-added',
        (message: HttpMessage) => this.onHttpMessageAdded_(message));
    this.browserProxy_.initialize();
  }

  /**
   * Triggers UpdateDevice RPC.
   */
  private onUpdateDeviceClicked_(): void {
    this.browserProxy_.updateDevice();
  }

  /**
   * Triggers ListContactPeople RPC.
   */
  private onListContactPeopleClicked_(): void {
    this.browserProxy_.listContactPeople();
  }

  /**
   * Triggers ListPublicCertificates RPC.
   */
  private onListPublicCertificatesClicked_(): void {
    this.browserProxy_.listPublicCertificates();
  }

  /**
   * Clears the |httpMessageList_| messages displayed on the page.
   */
  private onClearMessagesButtonClicked_(): void {
    this.httpMessageList_ = [];
  }

  /**
   * Adds a HTTP message to the javascript message list displayed. Called from
   * the C++ WebUI handler when a HTTP message is created in response to a Rpc.
   */
  private onHttpMessageAdded_(message: HttpMessage): void {
    this.unshift('httpMessageList_', message);
  }
}

customElements.define(HttpTabElement.is, HttpTabElement);
