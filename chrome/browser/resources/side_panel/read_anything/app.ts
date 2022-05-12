// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentNode, ContentType} from './read_anything.mojom-webui.js';
import {ReadAnythingApiProxy, ReadAnythingApiProxyImpl} from './read_anything_api_proxy.js';

const ReadAnythingElementBase = WebUIListenerMixin(PolymerElement);

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      fontName_: {
        type: String,
      }
    };
  }

  private apiProxy_: ReadAnythingApiProxy =
      ReadAnythingApiProxyImpl.getInstance();
  private listenerIds_: number[];
  private fontName_: string;

  // Defines the valid font names that can be passed to front-end and maps
  // them to a corresponding class style in app.html. Must stay in-sync with
  // the names set in read_anything_font_model.cc.
  private defaultFontName: string = 'standard';
  private validFontNames: {name: string, cssClass: string}[] = [
    {name: 'Standard', cssClass: 'standard'},
    {name: 'Serif', cssClass: 'serif'},
    {name: 'Sans-serif', cssClass: 'sans-serif'},
    {name: 'Arial', cssClass: 'arial'},
    {name: 'Open Sans', cssClass: 'open-sans'},
    {name: 'Calibri', cssClass: 'calibri'}
  ];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_ = [
      callbackRouter.showContent.addListener(
          (contentNodes: ContentNode[]) => this.showContent_(contentNodes)),

      callbackRouter.onFontNameChange.addListener(
          (newFontName: string) => this.updateFontName_(newFontName))
    ];
    this.apiProxy_.onUIReady();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  private buildNode_(contentNode: ContentNode): Node|null {
    switch (contentNode.type) {
      case ContentType.kHeading:
        return this.buildHeadingElement_(contentNode);
      case ContentType.kLink:
        return this.buildLinkElement_(contentNode);
      case ContentType.kParagraph:
        return this.buildParagraphElement_(contentNode);
      case ContentType.kStaticText:
        return this.buildStaticTextElement_(contentNode);
      default:
        return null;
    }
  }

  private buildHeadingElement_(contentNode: ContentNode): HTMLHeadingElement {
    let headingLevel: number = contentNode.headingLevel;
    // In ARIA 1.1, the default heading level is 2.
    // See AXNodeObject::kDefaultHeadingLevel.
    if (headingLevel < 1 || headingLevel > 6) {
      headingLevel = 2;
    }
    const tagName: string = 'h' + headingLevel;
    const element: HTMLElement = document.createElement(tagName);
    element.setAttribute('align', 'left');
    this.appendChildNodes_(contentNode, element);
    return element as HTMLHeadingElement;
  }

  private buildLinkElement_(contentNode: ContentNode): HTMLAnchorElement|null {
    if (!contentNode.url.url) {
      return null;
    }
    const element: HTMLAnchorElement = document.createElement('a');
    element.setAttribute('href', contentNode.url.url);
    this.appendChildNodes_(contentNode, element);
    return element;
  }

  private buildParagraphElement_(contentNode: ContentNode):
      HTMLParagraphElement {
    const element: HTMLParagraphElement = document.createElement('p');
    this.appendChildNodes_(contentNode, element);
    return element;
  }

  private buildStaticTextElement_(contentNode: ContentNode): Text|null {
    if (!contentNode.text || contentNode.children.length) {
      return null;
    }
    return document.createTextNode(contentNode.text);
  }

  private appendChildNodes_(contentNode: ContentNode, node: Node) {
    for (const childContentNode of contentNode.children) {
      const childNode: Node|null = this.buildNode_(childContentNode);
      if (childNode) {
        node.appendChild(childNode);
      }
    }
  }

  ////////////////////////////////////////////////////////////
  // Called by ReadAnythingPageHandler via callback router. //
  ////////////////////////////////////////////////////////////

  showContent_(contentNodes: ContentNode[]) {
    const shadowRoot: ShadowRoot|null = this.shadowRoot;
    if (!shadowRoot) {
      return;
    }
    const container: HTMLElement|null = shadowRoot.getElementById('container');
    if (!container) {
      return;
    }

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    container.replaceChildren();

    // Construct a dom node corresponding to each ContentNode and append it to
    // container. This does not use polymer's templating abstraction, which
    // would create a shadow node element representing each ContentNode, because
    // experimentation found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    for (const contentNode of contentNodes) {
      const node: Node|null = this.buildNode_(contentNode);
      if (node) {
        container.appendChild(node);
      }
    }
  }

  updateFontName_(newFontName: string) {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName = this.validFontNames.find(
        (f: {name: string, cssClass: string}) => f.name === newFontName);
    if (!validFontName) {
      this.fontName_ = this.defaultFontName;
    } else {
      this.fontName_ = validFontName.cssClass;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
