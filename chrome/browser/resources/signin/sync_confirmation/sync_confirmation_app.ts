/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './icons.html.js';
import './strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sync_confirmation_app.css.js';
import {getHtml} from './sync_confirmation_app.html.js';
import {ScreenMode} from './sync_confirmation_browser_proxy.js';
import type {SyncBenefit, SyncConfirmationBrowserProxy} from './sync_confirmation_browser_proxy.js';
import {SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';


interface AccountInfo {
  src: string;
  showEnterpriseBadge: boolean;
}

const SyncConfirmationAppElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SyncConfirmationAppElement extends SyncConfirmationAppElementBase {
  static get is() {
    return 'sync-confirmation-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      accountImageSrc_: {type: String},
      anyButtonClicked_: {type: Boolean},
      isModalDialog_: {type: Boolean},
      showEnterpriseBadge_: {type: Boolean},
      syncBenefitsList_: {type: Array},

      /**
       * Whether to show the new UI for Browser Sync Settings and which include
       * sublabel and Apps toggle shared between Ash and Lacros.
       */
      useClickableSyncInfoDesc_: {type: Boolean},

      /** Determines the screen mode. */
      screenMode_: {type: Number},
    };
  }

  protected accountImageSrc_: string =
      loadTimeData.getString('accountPictureUrl');
  protected anyButtonClicked_: boolean = false;
  protected isModalDialog_: boolean = loadTimeData.getBoolean('isModalDialog');
  private showEnterpriseBadge_: boolean = false;
  protected syncBenefitsList_: SyncBenefit[] =
      JSON.parse(loadTimeData.getString('syncBenefitsList'));
  private syncConfirmationBrowserProxy_: SyncConfirmationBrowserProxy =
      SyncConfirmationBrowserProxyImpl.getInstance();
  protected useClickableSyncInfoDesc_: boolean =
      loadTimeData.getBoolean('useClickableSyncInfoDesc');
  private screenMode_: ScreenMode = ScreenMode.PENDING;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'account-info-changed', this.handleAccountInfoChanged_.bind(this));
    this.addWebUiListener(
        'screen-mode-changed', this.handleScreenModeChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountInfo();
  }

  protected onConfirm_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]),
        this.screenMode_);
  }

  protected onUndo_() {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.undo(this.screenMode_);
  }

  protected onGoToSettings_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]),
        this.screenMode_);
  }

  /**
   * @param path Path of the click event. Must contain a consent confirmation
   *     element.
   * @return The text of the consent confirmation element.
   */
  private getConsentConfirmation_(path: HTMLElement[]): string {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.textContent!.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
  }

  /** @return Text of the consent description elements. */
  private getConsentDescription_(): string[] {
    const consentDescription =
        Array.from(this.shadowRoot!.querySelectorAll('[consent-description]'))
            .filter(
                element => element.getBoundingClientRect().width *
                        element.getBoundingClientRect().height >
                    0)
            .map(
                element => element.hasAttribute('localized-string') ?
                    element.getAttribute('localized-string')! :
                    element.textContent!.trim());

    assert(consentDescription.length);
    return consentDescription;
  }

  // Called when the account information changes: it might be either the image
  // or determined mode of screen restriction (derived from the
  // canShowHistorySyncOptInsWithoutMinorModeRestriction capability).
  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountImageSrc_ = accountInfo.src;
    this.showEnterpriseBadge_ = accountInfo.showEnterpriseBadge;
  }

  private handleScreenModeChanged_(screenMode: ScreenMode) {
    this.screenMode_ = screenMode;
  }

  protected getConfirmButtonClass_(): string {
    // TODO(b/326912202): Replace with observer pattern on screenMode_.
    switch (this.screenMode_) {
      case ScreenMode.UNRESTRICTED:
        return 'action-button';
      case ScreenMode.PENDING:
        return 'visibility-hidden';
      default:
        // All other cases have no special appearance.
        return '';
    }
  }

  protected getNotNowButtonClass_(): string {
    return this.screenMode_ === ScreenMode.PENDING ? 'visibility-hidden' : '';
  }

  protected isPending_(): boolean {
    return this.screenMode_ === ScreenMode.PENDING;
  }

  protected shouldHideEnterpriseBadge_(): boolean {
    return !this.showEnterpriseBadge_ ||
        this.screenMode_ === ScreenMode.PENDING;
  }

  /**
   * Called when the link to the device's sync settings is clicked.
   */
  protected onDisclaimerClicked_(event: CustomEvent<{event: Event}>) {
    // Prevent the default link click behavior.
    event.detail.event.preventDefault();

    // Programmatically open device's sync settings.
    this.syncConfirmationBrowserProxy_.openDeviceSyncSettings();
  }

  /**
   * Returns the name of class to apply on some tags to enable animations.
   * May be empty if no animations should be added.
   */
  protected getAnimationClass_(): string {
    return !this.isModalDialog_ ? 'fade-in' : '';
  }

  /**
   * Returns either "dialog" or an empty string.
   *
   * The returned value is intended to be added as a class on the root tags of
   * the element. Some styles from `tangible_sync_style_shared.css` rely on the
   * presence of this "dialog" class.
   */
  protected getMaybeDialogClass_(): string {
    return this.isModalDialog_ ? 'dialog' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sync-confirmation-app': SyncConfirmationAppElement;
  }
}

customElements.define(
    SyncConfirmationAppElement.is, SyncConfirmationAppElement);
