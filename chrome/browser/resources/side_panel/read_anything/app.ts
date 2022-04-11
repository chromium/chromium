// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadAnythingApiProxy} from './read_anything_api_proxy.js';

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
      paragraphs_: {
        type: Array,
        value: () => [],
      },

      fontName: {
        type: String,
      }
    };
  }

  private apiProxy_: ReadAnythingApiProxy = ReadAnythingApiProxy.getInstance();
  private listenerIds_: number[];
  private paragraphs_: string[];
  private fontName: string;

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
      callbackRouter.onEssentialContent.addListener(
          (essentialContent: string[]) =>
              this.showEssentialContent_(essentialContent)),

      callbackRouter.onFontNameChange.addListener(
          (newFontName: string) => this.updateFontName_(newFontName))
    ];
    this.apiProxy_.showUI();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  showEssentialContent_(essentialContent: string[]) {
    this.paragraphs_ = essentialContent;
  }

  updateFontName_(newFontName: string) {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName = this.validFontNames.find(
        (f: {name: string, cssClass: string}) => f.name === newFontName);
    if (!validFontName) {
      this.fontName = this.defaultFontName;
    } else {
      this.fontName = validFontName.cssClass;
    }
  }
}
customElements.define(ReadAnythingElement.is, ReadAnythingElement);
