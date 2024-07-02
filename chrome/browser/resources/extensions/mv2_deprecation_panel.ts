// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ItemDelegate} from './item.js';
import type {Mv2DeprecationDelegate} from './mv2_deprecation_delegate.js';
import {getTemplate} from './mv2_deprecation_panel.html.js';
import {Mv2ExperimentStage} from './mv2_deprecation_util.js';

export interface ExtensionsMv2DeprecationPanelElement {
  $: {
    actionMenu: CrActionMenuElement,
  };
}

export class ExtensionsMv2DeprecationPanelElement extends I18nMixin
(PolymerElement) {
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

      /*
       * Current Manifest V2 experiment stage.
       */
      mv2ExperimentStage: Number,

      /**
       * Whether the panel title should be shown.
       */
      showTitle: Boolean,

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
  delegate:
    ItemDelegate&Mv2DeprecationDelegate;
  mv2ExperimentStage: Mv2ExperimentStage;
  showTitle: boolean;
  private headerString_: string;
  private subtitleString_: string;
  private extensionWithActionMenuOpened_: chrome.developerPrivate.ExtensionInfo;

  /**
   * Updates properties after extensions change.
   */
  private async onExtensionsChanged_(): Promise<void> {
    let headerVar: string;
    let subtitleVar: string;
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        headerVar = 'mv2DeprecationPanelWarningHeader';
        subtitleVar = 'mv2DeprecationPanelWarningSubtitle';
        break;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        headerVar = 'mv2DeprecationPanelDisabledHeader';
        subtitleVar = 'mv2DeprecationPanelDisabledSubtitle';
        break;
      default:
        assertNotReached();
    }

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            headerVar, this.extensions.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            subtitleVar, this.extensions.length);
  }

  /**
   * Returns whether the dismiss button should be displayed.
   */
  private showDismissButton_(): boolean {
    // Button is only visible for the warning stage.
    // TODO(crbug.com/339061151): Button should also be visible for the
    // disabled stage (and have a different callback).
    return this.mv2ExperimentStage === Mv2ExperimentStage.WARNING;
  }

  /**
   * Returns whether the extensions find alternative button should be
   * displayed.
   */
  private showExtensionFindAlternativeButton_(
      extension: chrome.developerPrivate.ExtensionInfo): boolean {
    // Button is only visible for the warning stage iff extension has a
    // recommendations url.
    return this.mv2ExperimentStage === Mv2ExperimentStage.WARNING &&
        !!extension.recommendationsUrl;
  }

  /**
   * Returns whether the extensions action menu button should be displayed.
   */
  private showExtensionActionMenuButton_(): boolean {
    // Button is only visible for the warning stage.
    // TODO(crbug.com/339061151): Button should also be visible for the
    // disabled stage (and have a different callback).
    return this.mv2ExperimentStage === Mv2ExperimentStage.WARNING;
  }

  /**
   * Returns the accessible label for the action menu button corresponding to
   * `extensionName`.
   */
  private getActionMenuButtonLabelFor_(extensionName: string): string {
    return this.i18n(
        'mv2DeprecationPanelExtensionActionMenuLabel', extensionName);
  }

  /**
   * Returns the HTML representation of the subtitle string. We need the HTML
   * representation instead of the string since the string holds a link.
   */
  private getSubtitleString_(): TrustedHTML {
    return sanitizeInnerHtml(this.subtitleString_);
  }

  /**
   * Returns the accessible label for the find alternative button
   * corresponding to `extensionName`.
   */
  private getFindAlternativeButtonLabelFor_(extensionName: string): string {
    return this.i18n(
        'mv2DeprecationPanelFindAlternativeButtonAccLabel', extensionName);
  }

  /**
   * Triggers the panel dismissal when the dismiss button is clicked.
   */
  private onDismissButtonClick_() {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.Dismissed');
    this.delegate.dismissMv2DeprecationWarning();
  }

  /**
   * Opens a URL in the Web Store with extensions recommendations for the
   * extension whose find alternative button is clicked.
   */
  private onFindAlternativeButtonClick_(
      event: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>): void {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.FindAlternativeForExtension');
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
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    this.extensionWithActionMenuOpened_ = event.model.item;
    this.$.actionMenu.showAt(event.target as HTMLElement);
  }

  /**
   * Triggers an extension removal when the remove button is clicked for an
   * extension.
   */
  private onRemoveExtensionActionClicked_(): void {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.RemoveExtension');
    this.$.actionMenu.close();
    this.delegate.deleteItem(this.extensionWithActionMenuOpened_.id);
  }

  /**
   * Dismisses the notice for a given extension in the warning experiment stage.
   * It will not be shown again during this stage.
   */
  private onKeepExtensionActionClick_(): void {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.DismissedForExtension');
    this.$.actionMenu.close();
    this.delegate.dismissMv2DeprecationNoticeForExtension(
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
