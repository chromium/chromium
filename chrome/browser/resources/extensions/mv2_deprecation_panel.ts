// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shared_style.css.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ItemDelegate} from './item.js';
import {getTemplate} from './mv2_deprecation_panel.html.js';

export interface Mv2DeprecationPanelDelegate {
  dismissMv2DeprecationWarning(): void;
  dismissMv2DeprecationWarningForExtension(id: string): void;
}

export interface ExtensionsMv2DeprecationPanelElement {
  $: {
    actionMenu: CrActionMenuElement,
  };
}

export class ExtensionsMv2DeprecationPanelElement extends PolymerElement {
  static get is() {
    return 'extensions-mv2-deprecation-panel';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

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

      /**
       * Extension which has its action menu opened.
       */
      extensionWithActionMenuOpened_: Object,
    };
  }

  static get observers() {
    return ['onExtensionsChanged_(extensions.*)'];
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];
  delegate: ItemDelegate&Mv2DeprecationPanelDelegate;
  private headerString_: string;
  private subtitleString_: string;
  private extensionWithActionMenuOpened_: chrome.developerPrivate.ExtensionInfo;

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

  /**
   * Triggers the panel dismissal when the dismiss button is clicked.
   */
  private onDismissButtonClick_() {
    this.delegate.dismissMv2DeprecationWarning();
  }

  /**
   * Opens a URL in the Web Store with extensions recommendations for the
   * extension whose find alternative button is clicked.
   */
  private onFindAlternativeButtonClick_(
      event: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>): void {
    const recommendationsUrl: string|undefined =
        event.model.item.recommendationsUrl;
    assert(!!recommendationsUrl);
    this.delegate.openUrl(recommendationsUrl);
  }

  /**
   * Opens the action menu for a specific extension when the action menu button
   * is clicked.
   */
  private onExtensionActionMenuClick_(
      event: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>): void {
    this.extensionWithActionMenuOpened_ = event.model.item;
    this.$.actionMenu.showAt(event.target as HTMLElement);
  }

  /**
   * Triggers an extension removal when the remove button is clicked for an
   * extension.
   */
  private onRemoveExtensionActionClicked_(): void {
    this.$.actionMenu.close();
    this.delegate.deleteItem(this.extensionWithActionMenuOpened_.id);
  }

  /**
   * Dismisses the warning for a given extension. It will not be shown again.
   */
  private onKeepExtensionActionClick_(): void {
    this.$.actionMenu.close();
    this.delegate.dismissMv2DeprecationWarningForExtension(
        this.extensionWithActionMenuOpened_.id);
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
