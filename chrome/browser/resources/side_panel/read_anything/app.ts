// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

const ReadAnythingElementBase = WebUIListenerMixin(PolymerElement);

////////////////////////////////////////////////////////////
// Called by ReadAnythingPageHandler via callback router. //
////////////////////////////////////////////////////////////

// The chrome.readAnything context is created by the ReadAnythingAppController
// which is only instantiated when the kReadAnything feature is enabled. This
// check if chrome.readAnything exists prevents runtime errors when the feature
// is disabled.
if (chrome.readAnything) {
  chrome.readAnything.updateFontName = function() {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateFontName();
  };

  chrome.readAnything.updateContent = function() {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateContent();
  };
}

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      fontName_: {
        type: String,
      }
    };
  }

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
    if (chrome.readAnything) {
      chrome.readAnything.onConnected();
    }
  }

  private buildNode_(nodeId: number): Node|null {
    if (chrome.readAnything.isHeading(nodeId)) {
      return this.buildHeadingElement_(nodeId);
    }
    if (chrome.readAnything.isLink(nodeId)) {
      return this.buildLinkElement_(nodeId);
    }
    if (chrome.readAnything.isParagraph(nodeId)) {
      return this.buildParagraphElement_(nodeId);
    }
    if (chrome.readAnything.isStaticText(nodeId)) {
      return this.buildStaticTextElement_(nodeId);
    }
    return null;
  }

  private buildHeadingElement_(nodeId: number): HTMLHeadingElement {
    let headingLevel = chrome.readAnything.getHeadingLevel(nodeId);
    // In ARIA 1.1, the default heading level is 2.
    // See AXNodeObject::kDefaultHeadingLevel.
    if (headingLevel < 1 || headingLevel > 6) {
      headingLevel = 2;
    }
    const tagName = 'h' + headingLevel;
    const element = document.createElement(tagName);
    element.setAttribute('align', 'left');
    this.appendChildNodes_(element, nodeId);
    return element as HTMLHeadingElement;
  }

  private buildLinkElement_(nodeId: number): HTMLAnchorElement|null {
    const url = chrome.readAnything.getUrl(nodeId);
    if (!url.length) {
      return null;
    }
    const element = document.createElement('a');
    element.setAttribute('href', url);
    this.appendChildNodes_(element, nodeId);
    return element;
  }

  private buildParagraphElement_(nodeId: number): HTMLParagraphElement {
    const element = document.createElement('p');
    this.appendChildNodes_(element, nodeId);
    return element;
  }

  private buildStaticTextElement_(nodeId: number): Text|null {
    const textContent = chrome.readAnything.getTextContent(nodeId);
    if (!textContent.length) {
      return null;
    }
    return document.createTextNode(textContent);
  }

  private appendChildNodes_(node: Node, nodeId: number) {
    for (const childNodeId of chrome.readAnything.getChildren(nodeId)) {
      const childNode = this.buildNode_(childNodeId);
      if (childNode) {
        node.appendChild(childNode);
      }
    }
  }

  updateContent() {
    const shadowRoot = this.shadowRoot;
    if (!shadowRoot) {
      return;
    }
    const container = shadowRoot.getElementById('container');
    if (!container) {
      return;
    }

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    container.replaceChildren();

    // Construct a dom node corresponding to each AXNode and append it to
    // container. This does not use polymer's templating abstraction, which
    // would create a shadow node element representing each AXNode, because
    // experimentation found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    for (const nodeId of chrome.readAnything.contentNodeIds) {
      const node = this.buildNode_(nodeId);
      if (node) {
        container.appendChild(node);
      }
    }
  }

  updateFontName() {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName = this.validFontNames.find(
        (f: {name: string, cssClass: string}) =>
            f.name === chrome.readAnything.fontName);
    this.fontName_ =
        validFontName ? validFontName.cssClass : this.defaultFontName;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
