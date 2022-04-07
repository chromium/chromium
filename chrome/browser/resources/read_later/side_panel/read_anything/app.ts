// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';

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

      font_name_: {
        type: String,
      }
    };
  }

  private apiProxy_: ReadAnythingApiProxy = ReadAnythingApiProxy.getInstance();
  private listenerIds_: number[];
  private paragraphs_: string[];
  private font_name_: string;

  // Defines the valid font names that can be passed to front-end and maps
  // them to a corresponding class style in app.html. Must stay in-sync with
  // the names set in read_anything_font_model.cc.
  private default_font_name: string = 'standard';
  private valid_font_names: {name: string, css_class: string}[] = [
    {name: 'Standard', css_class: 'standard'},
    {name: 'Serif', css_class: 'serif'},
    {name: 'Sans-serif', css_class: 'sans-serif'},
    {name: 'Arial', css_class: 'arial'},
    {name: 'Open Sans', css_class: 'open-sans'},
    {name: 'Calibri', css_class: 'calibri'}
  ];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_ = [
      callbackRouter.onEssentialContent.addListener(
          (essentialContent: string[]) =>
              this.showEssentialContent_(essentialContent)),

      callbackRouter.onFontNameChange.addListener(
          (new_font_name: string) => this.updateFontName_(new_font_name))
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

  updateFontName_(new_font_name: string) {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName = this.valid_font_names.find(
        (f: {name: string, css_class: string}) => f.name === new_font_name);
    if (!validFontName) {
      this.font_name_ = this.default_font_name;
    } else {
      this.font_name_ = validFontName.css_class;
    }
  }
}
customElements.define(ReadAnythingElement.is, ReadAnythingElement);
