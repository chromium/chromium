// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Family Link Notice screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './family_link_notice.html.js';


export const FamilyLinkScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));


export class FamilyLinkNotice extends FamilyLinkScreenElementBase {
  static get is() {
    return 'family-link-notice-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * If the gaia account is newly created
       */
      isNewGaiaAccount_: {
        type: Boolean,
        value: false,
      },

      /**
       * The email address to be displayed
       */
      email_: {
        type: String,
        value: '',
      },

      /**
       * The enterprise domain to be displayed
       */
      domain_: {
        type: String,
        value: '',
      },
    };
  }

  private isNewGaiaAccount_: boolean;
  private email_: string;
  private domain_: string;

  override get EXTERNAL_API(): string[] {
    return [
      'setDisplayEmail',
      'setDomain',
      'setIsNewGaiaAccount',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FamilyLinkNoticeScreen');
  }

  /**
   * Returns default event target element.
   */
  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#familyLinkDialog');
  }

  /**
   * Sets email address.
   */
  setDisplayEmail(email: string): void {
    this.email_ = email;
  }

  /**
   * Sets enterprise domain.
   */
  setDomain(domain: string): void {
    this.domain_ = domain;
  }

  /**
   * Sets if the gaia account is newly created.
   */
  setIsNewGaiaAccount(isNewGaiaAccount: boolean): void {
    this.isNewGaiaAccount_ = isNewGaiaAccount;
  }

  /**
   * Returns the title of the dialog based on if account is managed. Account is
   * managed when email or domain field is not empty and we show parental
   * controls is not eligible.
   */
  private getDialogTitle_(locale: string, email: string, domain: string):
      string {
    if (email || domain) {
      return this.i18nDynamic(locale, 'familyLinkDialogNotEligibleTitle');
    } else {
      return this.i18nDynamic(locale, 'familyLinkDialogTitle');
    }
  }

  /**
   * Formats and returns the subtitle of the dialog based on if account is
   * managed or if account is newly created. Account is managed when email or
   * domain field is not empty and we show parental controls is not eligible.
   */
  private getDialogSubtitle_(
      locale: string, isNewGaiaAccount: boolean, email: string,
      domain: string): string {
    if (email || domain) {
      return this.i18n('familyLinkDialogNotEligibleSubtitle', email, domain);
    } else {
      if (isNewGaiaAccount) {
        return this.i18nDynamic(
            locale, 'familyLinkDialogNewGaiaAccountSubtitle');
      } else {
        return this.i18nDynamic(
            locale, 'familyLinkDialogExistingGaiaAccountSubtitle');
      }
    }
  }

  /**
   * On-tap event handler for Continue button.
   *
   */
  private onContinueButtonPressed_(): void {
    this.userActed('continue');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FamilyLinkNotice.is]: FamilyLinkNotice;
  }
}

customElements.define(FamilyLinkNotice.is, FamilyLinkNotice);
