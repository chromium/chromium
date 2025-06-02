// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './customize_buttons.css.js';
import {getHtml} from './customize_buttons.html.js';

export class CustomizeButtonsElement extends CrLitElement {
  static get is() {
    return 'ntp-customize-buttons';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      infoShownToUser: {
        reflect: true,
        type: Boolean,
      },
      modulesShownToUser: {
        reflect: true,
        type: Boolean,
      },
      showBackgroundImage: {
        reflect: true,
        type: Boolean,
      },
      showCustomize: {type: Boolean},
      showCustomizeChromeText: {type: Boolean},
      showWallpaperSearch: {
        reflect: true,
        type: Boolean,
      },
      showWallpaperSearchButton: {
        reflect: true,
        type: Boolean,
      },
      wallpaperSearchButtonAnimationEnabled: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  protected accessor infoShownToUser: boolean = false;
  protected accessor modulesShownToUser: boolean = false;
  protected accessor showBackgroundImage: boolean = false;
  protected accessor showCustomize: boolean = false;
  protected accessor showCustomizeChromeText: boolean = false;
  protected accessor showWallpaperSearch: boolean = false;
  protected accessor showWallpaperSearchButton: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    FocusOutlineManager.forDocument(document);
  }

  protected onCustomizeClick_() {
    this.fire('customize-click');
  }

  protected onWallpaperSearchClick_() {
    this.fire('wallpaper-search-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-customize-buttons': CustomizeButtonsElement;
  }
}

customElements.define(CustomizeButtonsElement.is, CustomizeButtonsElement);
