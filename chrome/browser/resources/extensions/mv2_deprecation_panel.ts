// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shared_style.css.js';

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './mv2_deprecation_panel.html.js';

export class ExtensionsMv2DeprecationPanelElement extends PolymerElement {
  static get is() {
    return 'extensions-mv2-deprecation-panel';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /*
       * Extensions to display in the panel.
       */
      extensions: {
        type: Array,
        notify: true,
      },

      /**
       * The string for the panel's header.
       */
      headerString_: String,

      /**
       * The string for the panel's subtitle.
       */
      subtitleString_: String,
    };
  }

  static get observers() {
    return ['onExtensionsChanged_(extensions.*)'];
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];
  private headerString_: string;
  private subtitleString_: string;

  /**
   * Updates properties after extensions change.
   */
  private async onExtensionsChanged_(): Promise<void> {
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'mv2DeprecationPanelWarningHeader', this.extensions.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'mv2DeprecationPanelWarningSubtitle', this.extensions.length);
  }

  /**
   * Returns the HTML representation of the subtitle string. We need the HTML
   * representation instead of the string since the string holds a link.
   */
  private getSubtitleString_(): TrustedHTML {
    return sanitizeInnerHtml(this.subtitleString_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-mv2-deprecation-panel': ExtensionsMv2DeprecationPanelElement;
  }
}

customElements.define(
    ExtensionsMv2DeprecationPanelElement.is,
    ExtensionsMv2DeprecationPanelElement);
