// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './footer.css.js';
import {getHtml} from './footer.html.js';
import type {Theme} from './new_tab_page.mojom-webui.js';
import {rgbaOrInherit} from './utils.js';

export interface FooterElement {
  $: {
    backgroundImageAttribution: HTMLElement,
    backgroundImageAttribution1: HTMLElement,
    backgroundImageAttribution2: HTMLElement,
    footerContainer: HTMLElement,
  };
}

export class FooterElement extends CrLitElement {
  static get is() {
    return 'ntp-footer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme: {type: Object},

      backgroundImageAttribution1_: {type: String},
      backgroundImageAttribution2_: {type: String},
      backgroundImageAttributionUrl_: {type: String},
    };
  }

  theme: Theme|null = null;
  protected backgroundImageAttribution1_: string;
  protected backgroundImageAttribution2_: string;
  protected backgroundImageAttributionUrl_: string;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('theme')) {
      this.backgroundImageAttribution1_ =
          this.computeBackgroundImageAttribution1_();
      this.backgroundImageAttribution2_ =
          this.computeBackgroundImageAttribution2_();
      this.backgroundImageAttributionUrl_ =
          this.computeBackgroundImageAttributionUrl_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('theme')) {
      this.onThemeChange_();
    }
  }

  private computeBackgroundImageAttribution1_(): string {
    return this.theme && this.theme.backgroundImageAttribution1 || '';
  }

  private computeBackgroundImageAttribution2_(): string {
    return this.theme && this.theme.backgroundImageAttribution2 || '';
  }

  private computeBackgroundImageAttributionUrl_(): string {
    return this.theme && this.theme.backgroundImageAttributionUrl ?
        this.theme.backgroundImageAttributionUrl.url :
        '';
  }

  private onThemeChange_() {
    if (this.theme) {
      this.style.setProperty(
          '--color-new-tab-page-attribution-foreground',
          rgbaOrInherit(this.theme.textColor));
      this.style.setProperty(
          '--color-new-tab-page-most-visited-foreground',
          rgbaOrInherit(this.theme.textColor));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-footer': FooterElement;
  }
}

customElements.define(FooterElement.is, FooterElement);
