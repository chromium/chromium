/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './icons.html.js';
import './strings.m.js';
import './signin_shared.css.js';
import './signin_vars.css.js';
import './tangible_sync_style_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sync_confirmation_app.html.js';
import type {SyncBenefit, SyncConfirmationBrowserProxy} from './sync_confirmation_browser_proxy.js';
import {SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';

// LINT.IfChange(screen_mode)
/**
 * In PENDING mode, the screen should not show consent buttons and indicate that
 * some loading is pending. In RESTRICTED mode, the button must not be weighted,
 * and in UNRESTRICTED mode they can be.
 *
 * In UNSUPPORTED mode, the client take any behavior.
 */
enum ScreenMode {
  UNSUPPORTED = 0,
  PENDING = 1,
  RESTRICTED = 2,
  UNRESTRICTED = 3,
}
// LINT.ThenChange(//chrome/browser/ui/webui/signin/sync_confirmation_handler.h:screen_mode)

interface AccountInfo {
  src: string;
  showEnterpriseBadge: boolean;
  screenMode: ScreenMode;
}


const SyncConfirmationAppElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SyncConfirmationAppElement extends SyncConfirmationAppElementBase {
  static get is() {
    return 'sync-confirmation-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      accountImageSrc_: {
        type: String,
        value() {
          return loadTimeData.getString('accountPictureUrl');
        },
      },

      anyButtonClicked_: {
        type: Boolean,
        value: false,
      },

      isModalDialog_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isModalDialog');
        },
      },

      showEnterpriseBadge_: {
        type: Boolean,
        value: false,
      },

      syncBenefitsList_: {
        type: Array,
        value() {
          return JSON.parse(loadTimeData.getString('syncBenefitsList'));
        },
      },

      /**
       * Whether to show the new UI for Browser Sync Settings and which include
       * sublabel and Apps toggle shared between Ash and Lacros.
       */
      useClickableSyncInfoDesc_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('useClickableSyncInfoDesc');
        },
      },

      /** Determines the screen mode. */
      screenMode_: {
        type: ScreenMode,
        value() {
          return loadTimeData.getInteger('screenMode') as ScreenMode;
        },
      },
    };
  }

  private accountImageSrc_: string;
  private anyButtonClicked_: boolean;
  private isModalDialog_: boolean;
  private showEnterpriseBadge_: boolean;
  private syncBenefitsList_: SyncBenefit[];
  private syncConfirmationBrowserProxy_: SyncConfirmationBrowserProxy =
      SyncConfirmationBrowserProxyImpl.getInstance();
  private useClickableSyncInfoDesc_: boolean;
  private screenMode_: ScreenMode;


  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'account-info-changed', this.handleAccountInfoChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountInfo();

    setTimeout(() => {
      this.defaultToRestrictedModeIfStillPending();
    }, /*delay in ms=*/ 2000);
  }

  private onConfirm_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]));
  }

  private onUndo_() {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.undo();
  }

  private onGoToSettings_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]));
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
        return element.innerHTML.trim();
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
                    element.innerHTML.trim());

    assert(consentDescription.length);
    return consentDescription;
  }

  // Called when the account information changes: it might be either the image
  // or determined mode of screen restriction (derived from the
  // canShowHistorySyncOptInsWithoutMinorModeRestriction capability).
  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountImageSrc_ = accountInfo.src;
    this.showEnterpriseBadge_ = accountInfo.showEnterpriseBadge;

    // Only allow this change once, from PENDING mode to (UN)RESTRICTED.
    if (this.screenMode_ === ScreenMode.PENDING) {
      this.screenMode_ = accountInfo.screenMode as ScreenMode;
    }
  }

  private defaultToRestrictedModeIfStillPending() {
    if (this.screenMode_ === ScreenMode.PENDING) {
      this.screenMode_ = ScreenMode.RESTRICTED;
    }
  }

  private getConfirmButtonClass_(screenMode: ScreenMode) {
    return screenMode === ScreenMode.UNRESTRICTED ? 'action-button' : '';
  }

  private isPending_(screenMode: ScreenMode) {
    return screenMode === ScreenMode.PENDING;
  }

  private shouldHideEnterpriseBadge_(
      screenMode: ScreenMode, showEnterpriseBadge: boolean) {
    return !showEnterpriseBadge || screenMode === ScreenMode.PENDING;
  }

  /**
   * Called when the link to the device's sync settings is clicked.
   */
  private onDisclaimerClicked_(event: CustomEvent<{event: Event}>) {
    // Prevent the default link click behavior.
    event.detail.event.preventDefault();

    // Programmatically open device's sync settings.
    this.syncConfirmationBrowserProxy_.openDeviceSyncSettings();
  }

  /**
   * Returns the name of class to apply on some tags to enable animations.
   * May be empty if no animations should be added.
   */
  private getAnimationClass_() {
    return !this.isModalDialog_ ? 'fade-in' : '';
  }

  /**
   * Returns either "dialog" or an empty string.
   *
   * The returned value is intended to be added as a class on the root tags of
   * the element. Some styles from `tangible_sync_style_shared.css` rely on the
   * presence of this "dialog" class.
   */
  private getMaybeDialogClass_() {
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
