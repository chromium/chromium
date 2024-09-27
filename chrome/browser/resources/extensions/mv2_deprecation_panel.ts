// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from './item.js';
import type {Mv2DeprecationDelegate} from './mv2_deprecation_delegate.js';
import {getCss} from './mv2_deprecation_panel.css.js';
import {getHtml} from './mv2_deprecation_panel.html.js';
import {Mv2ExperimentStage} from './mv2_deprecation_util.js';

export interface ExtensionsMv2DeprecationPanelElement {
  $: {
    actionMenu: CrActionMenuElement,
  };
}

const ExtensionsMv2DeprecationPanelElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsMv2DeprecationPanelElement extends
    ExtensionsMv2DeprecationPanelElementBase {
  static get is() {
    return 'extensions-mv2-deprecation-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},

      /*
       * Extensions to display in the panel.
       */
      extensions: {type: Array},

      /*
       * Current Manifest V2 experiment stage.
       */
      mv2ExperimentStage: {type: Number},

      /**
       * Whether the panel title should be shown.
       */
      showTitle: {type: Boolean},

      /**
       * The string for the panel's header.
       */
      headerString_: {type: String},

      /**
       * The string for the panel's subtitle.
       */
      subtitleString_: {type: String},

      /**
       * Extension which has its action menu opened.
       */
      extensionWithActionMenuOpened_: {type: Object},
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  delegate?: ItemDelegate&Mv2DeprecationDelegate;
  mv2ExperimentStage: Mv2ExperimentStage = Mv2ExperimentStage.NONE;
  showTitle: boolean = false;
  protected headerString_: string = '';
  private subtitleString_: string = '';
  private extensionWithActionMenuOpened_?:
      chrome.developerPrivate.ExtensionInfo;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('extensions')) {
      this.onExtensionsChanged_();
    }
  }

  /**
   * Updates properties after extensions change.
   */
  private async onExtensionsChanged_(): Promise<void> {
    let headerVar: string;
    let subtitleVar: string;
    let subtitleLink: string;
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        headerVar = 'mv2DeprecationPanelWarningHeader';
        subtitleVar = 'mv2DeprecationPanelWarningSubtitle';
        subtitleLink = 'https://chromewebstore.google.com/category/extensions';
        break;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        headerVar = 'mv2DeprecationPanelDisabledHeader';
        subtitleVar = 'mv2DeprecationPanelDisabledSubtitle';
        subtitleLink = 'https://support.google.com/chrome_webstore?' +
            'p=unsupported_extensions';
        break;
      default:
        assertNotReached();
    }

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            headerVar, this.extensions.length);
    const subtitle = await PluralStringProxyImpl.getInstance().getPluralString(
        subtitleVar, this.extensions.length);
    this.subtitleString_ = subtitle.replace('$1', subtitleLink);
  }

  /**
   * Returns whether the extension's find alternative button should be
   * displayed.
   */
  protected showExtensionFindAlternativeButton_(
      extension: chrome.developerPrivate.ExtensionInfo): boolean {
    // Button is only visible for the warning stage iff extension has a
    // recommendations url.
    return this.mv2ExperimentStage === Mv2ExperimentStage.WARNING &&
        !!extension.recommendationsUrl;
  }

  /**
   * Returns whether the extension's remove button should be displayed.
   */
  protected showExtensionRemoveButton_(
      extension: chrome.developerPrivate.ExtensionInfo): boolean {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        return false;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return !extension.mustRemainInstalled;
    }
  }

  /**
   * Returns whether the extension's action menu button should be displayed.
   */
  protected showActionMenu_(extension: chrome.developerPrivate.ExtensionInfo):
      boolean {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        return true;
      case Mv2ExperimentStage.UNSUPPORTED:
        return !!extension.recommendationsUrl;
    }
  }

  /**
   * Returns whether the find alternative button in the extension's action menu
   * should be displayed.
   */
  protected showExtensionFindAlternativeAction_(): boolean {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        return false;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return !!this.extensionWithActionMenuOpened_ &&
            !!this.extensionWithActionMenuOpened_.recommendationsUrl;
    }
  }

  /**
   * Returns whether the keep button in the extension's action menu should be
   * displayed.
   */
  protected showExtensionKeepAction_(): boolean {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        return true;
      case Mv2ExperimentStage.UNSUPPORTED:
        return false;
    }
  }

  /**
   * Returns whether the remove button in the extension's action menu should be
   * displayed.
   */
  protected showExtensionRemoveAction_(): boolean {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        return !!this.extensionWithActionMenuOpened_ &&
            !this.extensionWithActionMenuOpened_.mustRemainInstalled;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return false;
    }
  }

  /**
   * Returns the accessible label for the remove button corresponding to
   * `extensionName`.
   */
  protected getRemoveButtonLabelFor_(extensionName: string): string {
    return this.i18n('mv2DeprecationPanelRemoveButtonAccLabel', extensionName);
  }

  /**
   * Returns the accessible label for the action menu button corresponding to
   * `extensionName`.
   */
  protected getActionMenuButtonLabelFor_(extensionName: string): string {
    return this.i18n(
        'mv2DeprecationPanelExtensionActionMenuLabel', extensionName);
  }

  /**
   * Returns the HTML representation of the subtitle string. We need the HTML
   * representation instead of the string since the string holds a link.
   */
  protected getSubtitleString_(): TrustedHTML {
    return sanitizeInnerHtml(this.subtitleString_);
  }

  /**
   * Returns the accessible label for the find alternative button
   * corresponding to `extensionName`.
   */
  protected getFindAlternativeButtonLabelFor_(extensionName: string): string {
    return this.i18n(
        'mv2DeprecationPanelFindAlternativeButtonAccLabel', extensionName);
  }

  /**
   * Triggers the MV2 deprecation notice dismissal when the dismiss button is
   * clicked.
   */
  protected onDismissButtonClick_() {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Warning.Dismissed');
        break;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Disabled.Dismissed');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Unsupported.Dismissed');
        break;
    }

    assert(this.delegate);
    this.delegate.dismissMv2DeprecationNotice();
  }

  /**
   * Opens a URL in the Web Store with extensions recommendations for the
   * extension whose find alternative button is clicked.
   */
  protected onFindAlternativeButtonClick_(event: Event): void {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
      'Extensions.Mv2Deprecation.Warning.FindAlternativeForExtension');
    const recommendationsUrl: string|undefined =
        (event.target as HTMLElement).dataset['recommendationsUrl'];
    assert(!!recommendationsUrl);
    assert(this.delegate);
    this.delegate.openUrl(recommendationsUrl);
  }

  /**
   * Triggers an extension removal when the remove button is clicked for an
   * extension.
   */
  protected onRemoveButtonClick_(event: Event): void {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        assertNotReached();
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Disabled.RemoveExtension');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Unsupported.RemoveExtension');
        break;
    }

    this.$.actionMenu.close();
    const id = (event.target as HTMLElement).dataset['id'];
    assert(!!id);
    assert(this.delegate);
    this.delegate.deleteItem(id);
  }

  /**
   * Opens the action menu for a specific extension when the action menu button
   * is clicked.
   */
  protected onExtensionActionMenuClick_(event: Event): void {
    const index = Number((event.target as HTMLElement).dataset['index']);
    this.extensionWithActionMenuOpened_ = this.extensions[index]!;
    this.$.actionMenu.showAt(
        event.target as HTMLElement,
        {anchorAlignmentY: AnchorAlignment.AFTER_END});
  }

  /**
   * Opens a URL in the Web Store with extension recommendations for the
   * extension whose find alternative action is clicked.
   */
  protected onFindAlternativeExtensionActionClick_(): void {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        assertNotReached();
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Disabled.FindAlternativeForExtensionV2');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Unsupported.FindAlternativeForExtension');
        break;
    }

    const recommendationsUrl: string|undefined =
        this.extensionWithActionMenuOpened_?.recommendationsUrl;
    assert(!!recommendationsUrl);
    assert(this.delegate);
    this.delegate.openUrl(recommendationsUrl);
  }

  /**
   * Triggers an extension removal when the remove button in the action menu
   * is clicked for an extension.
   */
  protected onRemoveExtensionActionClicked_(): void {
    assert(this.mv2ExperimentStage === Mv2ExperimentStage.WARNING);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.RemoveExtension');
    this.$.actionMenu.close();
    assert(this.delegate);
    assert(this.extensionWithActionMenuOpened_);
    this.delegate.deleteItem(this.extensionWithActionMenuOpened_.id);
  }

  /**
   * Dismisses the notice for a given extension for the rest of the stage
   * duration.
   */
  protected onKeepExtensionActionClick_(): void {
    switch (this.mv2ExperimentStage) {
      case Mv2ExperimentStage.NONE:
        assertNotReached();
      case Mv2ExperimentStage.WARNING:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Warning.DismissedForExtension');
        break;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Disabled.DismissedForExtension');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        // TODO(crbug.com/339061151): Handle button for this stage.
        assertNotReached();
    }

    this.$.actionMenu.close();
    assert(this.delegate);
    assert(this.extensionWithActionMenuOpened_);
    this.delegate.dismissMv2DeprecationNoticeForExtension(
        this.extensionWithActionMenuOpened_.id);
  }
}

// Exported to be used in the autogenerated Lit template file
export type Mv2DeprecationPanelElement = ExtensionsMv2DeprecationPanelElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-mv2-deprecation-panel': ExtensionsMv2DeprecationPanelElement;
  }
}

customElements.define(
    ExtensionsMv2DeprecationPanelElement.is,
    ExtensionsMv2DeprecationPanelElement);
