/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncConfirmationBrowserProxy, SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';


type AccountInfo = {
  src: string,
  showEnterpriseBadge: boolean,
};

const SyncConfirmationAppElementBase = WebUIListenerMixin(PolymerElement);

export class SyncConfirmationAppElement extends SyncConfirmationAppElementBase {
  static get is() {
    return 'sync-confirmation-app';
  }

  static get template() {
    return html`{__html_template__}`;
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

      isNewDesignModalDialog_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('isModalDialog') &&
              loadTimeData.getBoolean('isNewDesign');
        }
      },

      isNewDesign_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isNewDesign');
        }
      },

      highlightColor_: {
        type: String,
        value() {
          if (!loadTimeData.valueExists('highlightColor')) {
            return '';
          }

          return loadTimeData.getString('highlightColor');
        }
      },

      showEnterpriseBadge_: {
        type: Boolean,
        value: false,
      },

      syncForced_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('syncForced');
        }
      },

      syncOptionalClass_: {
        type: String,
        value() {
          if (loadTimeData.getBoolean('syncForced')) {
            return '';
          }
          return 'sync-optional';
        },
      },
    };
  }

  private accountImageSrc_: string;
  private anyButtonClicked_: boolean;
  private isNewDesignModalDialog_: boolean;
  private isNewDesign_: boolean;
  private highlightColor_: string;
  private showEnterpriseBadge_: boolean;
  private syncForced_: boolean;
  private syncOptionalClass_: string;
  private syncConfirmationBrowserProxy_: SyncConfirmationBrowserProxy =
      SyncConfirmationBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'account-info-changed', this.handleAccountInfoChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountInfo();
  }

  private onConfirm_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as Array<HTMLElement>));
  }

  private onUndo_() {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.undo();
  }

  private onGoToSettings_(e: Event) {
    this.anyButtonClicked_ = true;
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as Array<HTMLElement>));
  }

  /**
   * @param path Path of the click event. Must contain a consent confirmation
   *     element.
   * @return The text of the consent confirmation element.
   */
  private getConsentConfirmation_(path: Array<HTMLElement>): string {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  }

  /** @return Text of the consent description elements. */
  private getConsentDescription_(): string[] {
    const consentDescription =
        Array.from(this.shadowRoot!.querySelectorAll('[consent-description]'))
            .filter(
                element => element.getBoundingClientRect().width *
                        element.getBoundingClientRect().height >
                    0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription.length);
    return consentDescription;
  }

  // Called when the account image changes.
  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountImageSrc_ = accountInfo.src;
    this.showEnterpriseBadge_ = accountInfo.showEnterpriseBadge;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sync-confirmation-app': SyncConfirmationAppElement;
  }
}

customElements.define(
    SyncConfirmationAppElement.is, SyncConfirmationAppElement);
