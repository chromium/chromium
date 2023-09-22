// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './wallpaper_search.html.js';

export interface WallpaperSearchElement {
  $: {
    heading: SpHeading,
    queryInput: CrInputElement,
    submitButton: CrButtonElement,
  };
}

export class WallpaperSearchElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-wallpaper-search';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      query_: String,
    };
  }

  private query_: string;

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private async onSearchClick_() {
    const {success} =
        await CustomizeChromeApiProxy.getInstance().handler.searchWallpaper(
            this.query_);
    this.$.queryInput.invalid = !success;
    this.$.queryInput.errorMessage = 'Error';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
