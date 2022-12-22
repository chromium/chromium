// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BackgroundCollection, CollectionImage, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './themes.html.js';

export interface ThemesElement {
  $: {
    backButton: HTMLButtonElement,
    header: HTMLElement,
  };
}

export class ThemesElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-themes';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedCollection: {
        type: Object,
        value: null,
        observer: 'onCollectionChange_',
      },
      themes_: Array,
      header_: String,
    };
  }

  private themes_: CollectionImage[];
  private header_: string;
  public selectedCollection: BackgroundCollection|null;

  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();
    FocusOutlineManager.forDocument(document);
  }

  private onCollectionChange_() {
    if (this.selectedCollection) {
      this.pageHandler_.getBackgroundImages(this.selectedCollection!.id)
          .then(({images}) => {
            this.themes_ = images;
          });
      this.header_ = this.selectedCollection.label;
    }
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onSelectTheme_(e: DomRepeatEvent<CollectionImage>) {
    const {
      attribution1,
      attribution2,
      attributionUrl,
      imageUrl,
      previewImageUrl,
    } = e.model.item;
    this.pageHandler_.setBackgroundImage(
        attribution1, attribution2, attributionUrl, imageUrl, previewImageUrl);
    this.dispatchEvent(new Event('theme-select'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-themes': ThemesElement;
  }
}

customElements.define(ThemesElement.is, ThemesElement);
