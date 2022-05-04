// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './heading_element.js';

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
      content_: {
        type: Array,
        value: () => [],
      },

      fontName_: {
        type: String,
      }
    };
  }

  private apiProxy_: ReadAnythingApiProxy =
      ReadAnythingApiProxyImpl.getInstance();
  private listenerIds_: number[];
  private content_: ContentNode[];
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

  /////////////////////////
  // Called by app.html. //
  /////////////////////////

  private isParagraph_(contentNode: ContentNode): boolean {
    return contentNode.type === ContentType.kParagraph;
  }

  private isHeading_(contentNode: ContentNode): boolean {
    return contentNode.type === ContentType.kHeading;
  }

  ////////////////////////////////////////////////////////////
  // Called by ReadAnythingPageHandler via callback router. //
  ////////////////////////////////////////////////////////////

  showContent_(contentNodes: ContentNode[]) {
    this.content_ = contentNodes;
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
