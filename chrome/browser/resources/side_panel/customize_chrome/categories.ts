// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './categories.html.js';
import {BackgroundCollection, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface CategoriesElement {
  $: {
    backButton: HTMLElement,
    classicChromeTile: HTMLElement,
  };
}

export class CategoriesElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-categories';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      collections_: Array,
    };
  }

  private collections_: BackgroundCollection[];

  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.pageHandler_.getBackgroundCollections().then(({collections}) => {
      this.collections_ = collections;
    });
  }

  private onClassicChromeClick_() {
    this.pageHandler_.setClassicChromeDefaultTheme();
    this.dispatchEvent(new Event('theme-select'));
  }

  private onCollectionClick_(e: DomRepeatEvent<BackgroundCollection>) {
    this.dispatchEvent(new CustomEvent<BackgroundCollection>(
        'collection-select', {detail: e.model.item}));
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-categories': CategoriesElement;
  }
}

customElements.define(CategoriesElement.is, CategoriesElement);
