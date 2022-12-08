// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

const ReadAnythingElementBase = WebUiListenerMixin(PolymerElement);

////////////////////////////////////////////////////////////
// Called by ReadAnythingPageHandler via callback router. //
////////////////////////////////////////////////////////////

// The chrome.readAnything context is created by the ReadAnythingAppController
// which is only instantiated when the kReadAnything feature is enabled. This
// check if chrome.readAnything exists prevents runtime errors when the feature
// is disabled.
if (chrome.readAnything) {
  chrome.readAnything.updateContent = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateContent();
  };

  chrome.readAnything.updateTheme = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateTheme();
  };
}

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return getTemplate();
  }

  // Defines the valid font names that can be passed to front-end and maps
  // them to a corresponding class style in app.html. Must stay in-sync with
  // the names set in read_anything_font_model.cc.
  // TODO(1266555): The displayed names of the fonts should be messages that
  //                will be translated to other languages.
  private defaultFontName: string = 'Standard font';
  private validFontNames: Array<{name: string, css: string}> = [
    {name: 'Standard font', css: 'Standard font'},
    {name: 'Sans-serif', css: 'sans-serif'},
    {name: 'Serif', css: 'serif'},
    {name: 'Avenir', css: 'avenir'},
    {name: 'Comic Neue', css: '"Comic Neue"'},
    {name: 'Comic Sans MS', css: '"Comic Sans MS"'},
    {name: 'Poppins', css: 'poppins'},
  ];

  override connectedCallback() {
    super.connectedCallback();
    if (chrome.readAnything) {
      chrome.readAnything.onConnected();
    }
  }

  private buildSubtree_(nodeId: number): Node {
    let htmlTag = chrome.readAnything.getHtmlTag(nodeId);
    // Text nodes do not have an html tag.
    if (!htmlTag.length) {
      const textContent = chrome.readAnything.getTextContent(nodeId);
      return document.createTextNode(textContent);
    }
    // getHtmlTag might return '#document' which is not a valid to pass to
    // createElement.
    if (htmlTag === '#document') {
      htmlTag = 'div';
    }

    const element = document.createElement(htmlTag);
    const direction = chrome.readAnything.getTextDirection(nodeId);
    if (direction) {
      element.setAttribute('dir', direction);
    }
    const url = chrome.readAnything.getUrl(nodeId);
    if (url && element.nodeName === 'A') {
      element.setAttribute('href', url);
      element.onclick = () => {
        chrome.readAnything.onLinkClicked(nodeId);
      };
    }
    const language = chrome.readAnything.getLanguage(nodeId);
    if (language) {
      element.setAttribute('lang', language);
    }
    this.appendChildSubtrees_(element, nodeId);
    return element;
  }

  private appendChildSubtrees_(node: Node, nodeId: number) {
    for (const childNodeId of chrome.readAnything.getChildren(nodeId)) {
      const childNode = this.buildSubtree_(childNodeId);
      node.appendChild(childNode);
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

    // Construct a dom subtree starting with the display root and append it to
    // the container. The display root may be invalid if there are no content
    // nodes and no selection.
    // This does not use polymer's templating abstraction, which
    // would create a shadow node element representing each AXNode, because
    // experimentation found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    const rootId = chrome.readAnything.rootId;
    if (!rootId) {
      return;
    }
    const node = this.buildSubtree_(rootId);
    container.appendChild(node);
  }

  validatedFontName(): string {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName = this.validFontNames.find(
        (f: {name: string}) => f.name === chrome.readAnything.fontName);
    return validFontName ? validFontName.css : this.defaultFontName;
  }

  updateTheme() {
    const foregroundColor:
        SkColor = {value: chrome.readAnything.foregroundColor};
    const backgroundColor:
        SkColor = {value: chrome.readAnything.backgroundColor};

    this.updateStyles({
      '--background-color': skColorToRgba(backgroundColor),
      '--font-family': this.validatedFontName(),
      '--font-size': chrome.readAnything.fontSize + 'em',
      '--foreground-color': skColorToRgba(foregroundColor),
      '--letter-spacing': chrome.readAnything.letterSpacing + 'em',
      '--line-height': chrome.readAnything.lineSpacing,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
