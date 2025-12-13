// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './customize_buttons.css.js';
import {getHtml} from './customize_buttons.html.js';

export interface CustomizeButtonsElement {
  $: {
    customizeButton: CrButtonElement,
  };
}

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
      showShadow: {
        reflect: true,
        type: Boolean,
      },
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

  accessor infoShownToUser: boolean = false;
  accessor modulesShownToUser: boolean = false;
  accessor showBackgroundImage: boolean = false;
  accessor showCustomize: boolean = false;
  accessor showCustomizeChromeText: boolean = false;
  accessor showShadow: boolean = false;
  accessor showWallpaperSearch: boolean = false;
  accessor showWallpaperSearchButton: boolean = false;

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
