// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {DictionaryValue} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getCss} from './slim_web_view.css.js';
import {getHtml} from './slim_web_view.html.js';
import {BrowserProxyImpl} from './slim_web_view_browser_proxy.js';

function dispatchEvent(
    eventName: string, _args: DictionaryValue, instanceId: number) {
  // TODO(crbug.com/460804848): Implement processing of events that aren't
  // dispatched to views.
  const view =
      chrome.slimWebViewPrivate.getViewFromId(instanceId) as SlimWebViewElement;
  if (!view) {
    console.warn(
        'Skipping event %s for instance id %d because it was not found.',
        eventName, instanceId);
    return;
  }
  // TODO(crbug.com/460804848): Implement passing of event fields.
  view.dispatchEvent(new Event(eventName));
}

window.addEventListener('load', () => {
  const proxy = BrowserProxyImpl.getInstance();
  proxy.callbackRouter.dispatchEvent.addListener(dispatchEvent);
});

export interface SlimWebViewElement {
  $: {
    input: HTMLElement,
  };
}

const GUEST_INSTANCE_ID_PENDING: number = 0;

export class SlimWebViewElement extends CrLitElement {
  static get is() {
    // TODO(crbug.com/460804848): Rename to webview, which is a restricted name.
    return 'slim-webview';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      src: {type: String, reflect: true},
    };
  }

  accessor src: string = '';

  contentWindow: WindowProxy|null = null;

  private viewInstanceId: number = chrome.slimWebViewPrivate.getNextId();
  private containerId: number|null = null;
  private guestInstanceId: number|null = null;

  constructor() {
    super();

    chrome.slimWebViewPrivate.registerView(this.viewInstanceId, this);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('src')) {
      if (this.guestInstanceId === null) {
        this.guestInstanceId = GUEST_INSTANCE_ID_PENDING;
        this.createGuest();
        return;
      }
      if (this.contentWindow === null) {
        return;
      }
      this.navigate();
    }
  }

  private async createGuest() {
    const createParams: DictionaryValue = {
      storage: {instanceId: {intValue: this.viewInstanceId}},
    };
    const result =
        await BrowserProxyImpl.getInstance().handler.createGuest(createParams);
    this.onGuestCreated(result.guestInstanceId);
    // TODO(crbug.com/460804848): destroy guest when this element is destroyed,
    // if guest is not attached.
  }

  private onGuestCreated(guestInstanceId: number) {
    assert(this.guestInstanceId === GUEST_INSTANCE_ID_PENDING);
    if (!guestInstanceId) {
      this.guestInstanceId = null;
      assertNotReached('Failed to create guest');
    }
    this.guestInstanceId = guestInstanceId;
    const params = {instanceId: this.viewInstanceId};
    const iframeElement = this.shadowRoot.querySelector('iframe');
    assert(iframeElement);
    assert(iframeElement.contentWindow);
    this.containerId = chrome.slimWebViewPrivate.getNextId();
    // TODO(crbug.com/460804848): Bind the destruction of the container to the
    // destruction of this element.
    chrome.slimWebViewPrivate.attachIframeGuest(
        this.containerId,
        this.guestInstanceId,
        params,
        iframeElement.contentWindow,
        () => {
          this.contentWindow = iframeElement.contentWindow;
          this.navigate();
        },
    );
  }

  private navigate() {
    if (!this.src) {
      return;
    }
    assert(this.guestInstanceId !== null);
    assert(this.guestInstanceId !== GUEST_INSTANCE_ID_PENDING);
    let url: URL;
    try {
      url = new URL(this.src);
    } catch (e) {
      assertNotReached(`Failed to parse URL for webview: ${e}`);
    }
    assert(url.protocol === 'https:' || url.href === 'about:blank');
    BrowserProxyImpl.getInstance().handler.navigate(
        this.guestInstanceId, url.href);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'slim-webview': SlimWebViewElement;
  }
}

customElements.define(SlimWebViewElement.is, SlimWebViewElement);
