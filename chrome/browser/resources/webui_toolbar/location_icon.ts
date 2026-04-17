// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './location_icon.css.js';
import {getHtml} from './location_icon.html.js';
import {SecurityChipIcon, SecurityLevel} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {SecurityChipState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class LocationIconElement extends CrLitElement {
  static get is() {
    return 'location-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
      clickable: {
        type: Boolean,
        reflect: true,
      },
      isDangerous: {
        type: Boolean,
        reflect: true,
        attribute: 'is-dangerous',
      },
      hasText: {
        type: Boolean,
        reflect: true,
        attribute: 'has-text',
      },
      isTextDangerous: {
        type: Boolean,
        reflect: true,
        attribute: 'is-text-dangerous',
      },
    };
  }

  accessor state: SecurityChipState = {
    icon: SecurityChipIcon.kUnspecified,
    securityLevel: 0,
    text: '',
    isClickable: false,
    isTextDangerous: false,
  };

  accessor clickable: boolean = false;

  // True when the SecurityLevel is kDangerous (e.g. expired cert). The text
  // might still be "Not secure" in this state, which kWarning also has.
  accessor isDangerous: boolean = false;

  // True if the chip should display text alongside the icon. This drives CSS
  // rules that manage the expanded pill shape and padding.
  accessor hasText: boolean = false;

  // True specifically when the text is exactly "Dangerous" (e.g. Malware).
  // This is a higher alert state than just isDangerous.
  accessor isTextDangerous: boolean = false;

  protected getIconUrl_(): string {
    switch (this.state.icon) {
      case SecurityChipIcon.kHttp:
        return 'url(lhs_icons/http_chrome_refresh.svg)';
      case SecurityChipIcon.kDangerous:
        return 'url(lhs_icons/dangerous_chrome_refresh.svg)';
      case SecurityChipIcon.kNotSecureWarning:
        return 'url(lhs_icons/not_secure_warning_chrome_refresh_16.svg)';
      case SecurityChipIcon.kSecurePageInfo:
      case SecurityChipIcon.kGoogleSuperG:
      case SecurityChipIcon.kGoogleGMonochrome:
      case SecurityChipIcon.kAddContext:
      default:
        // Fallbacks for missing Google/Context icons.
        // TODO(crbug.com/495419742): Add SVGs for missing icons.
        return 'url(lhs_icons/secure_page_info_chrome_refresh.svg)';
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      this.clickable = this.state.isClickable;
      this.isDangerous = this.state.securityLevel === SecurityLevel.kDangerous;
      this.hasText = !!this.state.text;
      this.isTextDangerous = this.state.isTextDangerous;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'location-icon': LocationIconElement;
  }
}

customElements.define(LocationIconElement.is, LocationIconElement);
