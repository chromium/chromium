// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-email-issuer-icon' displays an EVP issuer's logo
 * downloaded via chrome://image, falling back to the issuer site's favicon
 * until the logo loads or if it is absent, non-HTTPS, or fails to load.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../site_favicon.js';

import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './email_issuer_icon.css.js';
import {getHtml} from './email_issuer_icon.html.js';

function sanitizeLogoUrl(logoUrl: string): string {
  if (!logoUrl) {
    return '';
  }
  try {
    const url = new URL(logoUrl);
    if (url.protocol !== 'https:' || url.username || url.password) {
      return '';
    }
    url.hash = '';
    return url.href;
  } catch {
    return '';
  }
}

export interface SettingsEmailIssuerIconElement {
  $: {
    logo: CrAutoImgElement,
  };
}

export class SettingsEmailIssuerIconElement extends CrLitElement {
  static get is() {
    return 'settings-email-issuer-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      issuerSite: {type: String},
      logoUrl: {type: String},
      sanitizedLogoUrl_: {type: String},
      showLogo_: {type: Boolean},
    };
  }

  accessor issuerSite: string = '';
  accessor logoUrl: string = '';
  protected accessor sanitizedLogoUrl_: string = '';
  private accessor showLogo_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('logoUrl')) {
      const nextUrl = sanitizeLogoUrl(this.logoUrl);
      if (nextUrl !== this.sanitizedLogoUrl_) {
        this.sanitizedLogoUrl_ = nextUrl;
        this.showLogo_ = false;
      }
    }
  }

  protected shouldShowLogo_(): boolean {
    return this.showLogo_ && Boolean(this.sanitizedLogoUrl_);
  }

  protected onLogoLoad_() {
    if (this.sanitizedLogoUrl_ &&
        this.$.logo.autoSrc === this.sanitizedLogoUrl_) {
      this.showLogo_ = true;
    }
  }

  protected onLogoError_() {
    this.showLogo_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-email-issuer-icon': SettingsEmailIssuerIconElement;
  }
}

customElements.define(
    SettingsEmailIssuerIconElement.is, SettingsEmailIssuerIconElement);
