// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {DictionaryValue} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {EventDispatcher} from './event_dispatcher.js';
import type {EventDict, EventMap} from './event_dispatcher.js';
import {getCss} from './slim_web_view.css.js';
import {getHtml} from './slim_web_view.html.js';
import {BrowserProxyImpl, PermissionResponseAction} from './slim_web_view_browser_proxy.js';

const GUEST_INSTANCE_ID_PENDING: number = 0;

export class ExitEvent extends Event {
  readonly reason: string;
  readonly processId: number;

  static factory(args: EventDict) {
    return new ExitEvent(args);
  }

  private constructor(args: EventDict) {
    super('exit', {
      bubbles: true,
      cancelable: false,
    });
    this.reason = args.getString('reason');
    this.processId = args.getInt('processId');
  }
}

export class LoadEvent extends Event {
  readonly url: string;
  readonly isTopLevel: boolean;

  static loadCommitFactory(args: EventDict) {
    return new LoadEvent('loadcommit', args);
  }

  static loadStartFactory(args: EventDict) {
    return new LoadEvent('loadstart', args);
  }

  protected constructor(type: string, args: EventDict) {
    super(type, {
      bubbles: true,
      cancelable: false,
    });
    this.url = args.getString('url');
    this.isTopLevel = args.getBool('isTopLevel');
  }
}

export class LoadAbortEvent extends LoadEvent {
  readonly code: number;
  readonly reason: string;

  static factory(args: EventDict) {
    return new LoadAbortEvent(args);
  }

  private constructor(args: EventDict) {
    super('loadabort', args);
    this.code = args.getInt('code');
    this.reason = args.getString('reason');
  }
}

export class NewWindowEvent extends Event {
  readonly targetUrl: string;
  readonly windowOpenDisposition: string;
  readonly initialWidth: number;
  readonly initialHeight: number;

  static factory(args: EventDict): NewWindowEvent {
    return new NewWindowEvent(args);
  }

  private constructor(args: EventDict) {
    super('newwindow', {
      bubbles: true,
      cancelable: true,
    });
    const requestInfo = args.getDict('requestInfo');
    this.initialWidth = requestInfo.getInt('initialWidth');
    this.initialHeight = requestInfo.getInt('initialHeight');
    this.targetUrl = requestInfo.getString('targetUrl');
    this.windowOpenDisposition = requestInfo.getString('windowOpenDisposition');
  }
}

export interface PermissionRequest {
  url: string;
  allow(): void;
  deny(): void;
}

export class PermissionRequestEvent extends Event {
  readonly permission: string;
  readonly request: PermissionRequest;
  private readonly requestId: number;
  private readonly guestInstanceId: number;
  private actionTaken: boolean = false;

  static factory(args: EventDict, guestInstanceId: number) {
    return new PermissionRequestEvent(args, guestInstanceId);
  }

  private constructor(args: EventDict, guestInstanceId: number) {
    super('permissionrequest', {
      bubbles: true,
      cancelable: true,
    });
    this.permission = args.getString('permission');
    this.requestId = args.getInt('requestId');
    this.guestInstanceId = guestInstanceId;
    const requestInfo = args.getDict('requestInfo');
    this.request = {
      url: requestInfo.getString('url'),
      allow: this.allow.bind(this),
      deny: this.deny.bind(this),
    };
  }

  handle(element: HTMLElement) {
    const performDefault = element.dispatchEvent(this);
    if (!performDefault || this.actionTaken) {
      // Because SlimWebView is only used in WebUIs, we assume that an action
      // is always taken by the event handler that chose to prevent the default
      // action. Note that the action might be taken asynchronously.
      return;
    }
    this.actionTaken = true;
    this.defaultAction();
  }

  private allow() {
    assert(!this.actionTaken);
    this.actionTaken = true;
    BrowserProxyImpl.getInstance().handler.setPermission(
        this.guestInstanceId, this.requestId, PermissionResponseAction.kAllow);
  }

  private deny() {
    assert(!this.actionTaken);
    this.actionTaken = true;
    BrowserProxyImpl.getInstance().handler.setPermission(
        this.guestInstanceId, this.requestId, PermissionResponseAction.kDeny);
  }

  private async defaultAction() {
    const result = await BrowserProxyImpl.getInstance().handler.setPermission(
        this.guestInstanceId, this.requestId,
        PermissionResponseAction.kDefault);
    if (!result.allowed) {
      console.warn(`Permission ${this.permission} denied`);
    }
  }
}


class SizeChangedEvent extends Event {
  readonly oldHeight: number;
  readonly oldWidth: number;
  readonly newHeight: number;
  readonly newWidth: number;

  static factory(args: EventDict) {
    return new SizeChangedEvent(args);
  }

  private constructor(args: EventDict) {
    super('sizechanged', {
      bubbles: true,
      cancelable: false,
    });
    this.oldHeight = args.getInt('oldHeight');
    this.oldWidth = args.getInt('oldWidth');
    this.newHeight = args.getInt('newHeight');
    this.newWidth = args.getInt('newWidth');
  }

  handle(element: HTMLElement) {
    assertInstanceof(element, SlimWebViewElement);
    const maxWidth = element.maxwidth || element.offsetWidth;
    const minWidth =
        Math.min(element.minwidth || element.offsetWidth, maxWidth);
    const maxHeight = element.maxheight || element.offsetHeight;
    const minHeight =
        Math.min(element.minheight || element.offsetHeight, maxHeight);

    if (!element.autosize ||
        (this.newWidth >= minWidth && this.newWidth <= maxWidth &&
         this.newHeight >= minHeight && this.newHeight <= maxHeight)) {
      element.style.width = `${this.newWidth}px`;
      element.style.height = `${this.newHeight}px`;
      element.dispatchEvent(this);
    }
  }
}

const eventDescriptors: EventMap = new Map([
  ['contentload', {}],
  [
    'exit',
    {
      factory: ExitEvent.factory,
    },
  ],
  [
    'loadabort',
    {
      factory: LoadAbortEvent.factory,
    },
  ],
  [
    'loadcommit',
    {
      factory: LoadEvent.loadCommitFactory,
    },
  ],
  [
    'loadstart',
    {
      factory: LoadEvent.loadStartFactory,
    },
  ],
  [
    'loadstop',
    {},
  ],
  [
    'newwindow',
    {
      factory: NewWindowEvent.factory,
    },
  ],
  [
    'permission',
    {
      factory: PermissionRequestEvent.factory,
      handler: PermissionRequestEvent.prototype.handle,
    },
  ],
  [
    'sizechanged',
    {
      factory: SizeChangedEvent.factory,
      handler: SizeChangedEvent.prototype.handle,
    },
  ],
  ['unresponsive', {}],
]);

const slimWebViewContainerFinalizationRegistry =
    new FinalizationRegistry((containerId: number) => {
      chrome.slimWebViewPrivate.destroyContainer(containerId);
    });

export class SlimWebViewElement extends CrLitElement {
  static get is() {
    // This is a restricted custom element name that is allowed-listed in
    // slim_web_view_bindings.cc.
    // TODO(crbug.com/487332297): Rename class to WebviewElement.
    return 'webview';
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
      autosize: {type: Boolean, reflect: true},
      minwidth: {type: Number, reflect: true},
      maxwidth: {type: Number, reflect: true},
      minheight: {type: Number, reflect: true},
      maxheight: {type: Number, reflect: true},
      partition: {type: String, reflect: true},
    };
  }

  accessor src: string = '';
  accessor autosize: boolean = false;
  accessor minwidth: number = 0;
  accessor maxwidth: number = 0;
  accessor minheight: number = 0;
  accessor maxheight: number = 0;
  accessor partition: string = '';

  contentWindow: WindowProxy|null = null;

  private viewInstanceId: number = chrome.slimWebViewPrivate.getNextId();
  private containerId: number|null = null;
  private guestInstanceId: number|null = null;
  private eventDispatcher: EventDispatcher|null = null;

  constructor() {
    super();

    chrome.slimWebViewPrivate.registerView(this.viewInstanceId, this);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.maybeSetupEventDispatcher();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.eventDispatcher !== null) {
      this.eventDispatcher.disconnect();
      this.eventDispatcher = null;
    }
  }

  override shouldUpdate(changedProperties: PropertyValues<this>): boolean {
    if (changedProperties.has('partition')) {
      if (!this.validatePartitionUpdate(changedProperties.get('partition'))) {
        changedProperties.delete('partition');
      }
    }
    return super.shouldUpdate(changedProperties);
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
    if (changedProperties.has('autosize') ||
        changedProperties.has('minwidth') ||
        changedProperties.has('maxwidth') ||
        changedProperties.has('minheight') ||
        changedProperties.has('maxheight')) {
      this.updateAutoSize();
    }
  }

  private async createGuest() {
    const css = getComputedStyle(this);
    const elementRect = this.getBoundingClientRect();
    const elementWidth =
        elementRect.width || parseInt(css.getPropertyValue('width'));
    const elementHeight =
        elementRect.height || parseInt(css.getPropertyValue('height'));
    const createParams: DictionaryValue = {
      storage: {
        instanceId: {intValue: this.viewInstanceId},
        elementWidth: {intValue: elementWidth},
        elementHeight: {intValue: elementHeight},
        // Attributes relevant to guest creation.
        autosize: {boolValue: this.autosize},
        minwidth: {intValue: this.minwidth},
        maxwidth: {intValue: this.maxwidth},
        minheight: {intValue: this.minheight},
        maxheight: {intValue: this.maxheight},
        partition: {stringValue: this.partition},
      },
    };
    const result =
        await BrowserProxyImpl.getInstance().handler.createGuest(createParams);
    // We immediately attach the guest to the embedder frame. The browser knows
    // how to handle the guest's lifecycle after this, so we don't need to
    // handle its destruction explicitly here.
    this.onGuestCreated(result.guestInstanceId);
  }

  private onGuestCreated(guestInstanceId: number) {
    assert(this.guestInstanceId === GUEST_INSTANCE_ID_PENDING);
    if (!guestInstanceId) {
      this.guestInstanceId = null;
      assertNotReached('Failed to create guest');
    }
    this.guestInstanceId = guestInstanceId;
    this.maybeSetupEventDispatcher();
    assert(this.eventDispatcher !== null);
    const params = {instanceId: this.viewInstanceId};
    const iframeElement = this.shadowRoot.querySelector('iframe');
    assert(iframeElement);
    assert(iframeElement.contentWindow);
    this.containerId = chrome.slimWebViewPrivate.getNextId();
    slimWebViewContainerFinalizationRegistry.register(this, this.containerId);
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

  private maybeSetupEventDispatcher() {
    if (this.guestInstanceId === null ||
        this.guestInstanceId === GUEST_INSTANCE_ID_PENDING ||
        this.eventDispatcher !== null) {
      return;
    }
    this.eventDispatcher = new EventDispatcher(
        eventDescriptors, this.viewInstanceId, this.guestInstanceId, this);
    this.eventDispatcher.connect();
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
    assert(
        url.protocol === 'https:' || url.protocol === 'http:' ||
        url.href === 'about:blank');
    BrowserProxyImpl.getInstance().handler.navigate(
        this.guestInstanceId, url.href);
  }

  private updateAutoSize() {
    if (this.guestInstanceId === null ||
        this.guestInstanceId === GUEST_INSTANCE_ID_PENDING) {
      return;
    }
    const params = {
      enableAutoSize: this.autosize,
      min: {
        width: this.minwidth,
        height: this.minheight,
      },
      max: {
        width: this.maxwidth,
        height: this.maxheight,
      },
    };
    BrowserProxyImpl.getInstance().handler.setSize(
        this.guestInstanceId, params);
  }

  private validatePartitionUpdate(oldPartition: string|undefined): boolean {
    if (this.guestInstanceId !== null) {
      this.partition = oldPartition || '';
      console.error(
          'partition attribute can\'t be changed after the first navigation');
      return false;
    }
    if (this.partition === 'persist:') {
      this.partition = oldPartition || '';
      console.error('invalid partition attribute');
      return false;
    }
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webview': SlimWebViewElement;
  }
}

chrome.slimWebViewPrivate.allowGuestViewElementDefinition(() => {
  customElements.define(SlimWebViewElement.is, SlimWebViewElement);
});
