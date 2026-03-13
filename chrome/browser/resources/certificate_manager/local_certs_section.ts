// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'local-certs-section' component is a section of the
 * Certificate Management UI that shows local modifications to the the users
 * trusted roots for TLS server auth (e.g. roots imported from the platform).
 */

import '/strings.m.js';
import './certificate_manager_icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertManagementMetadata} from './certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from './certificates_browser_proxy.js';
import {getCss} from './local_certs_section.css.js';
import {getHtml} from './local_certs_section.html.js';
import {Page, Router} from './navigation.js';

export interface LocalCertsSectionElement {
  $: {
    // <if expr="is_win or is_macosx">
    manageOsImportedCerts: HTMLElement,
    // </if>

    // <if expr="not is_chromeos">
    importOsCerts: CrToggleElement,
    importOsCertsManagedIcon: HTMLElement,
    viewOsImportedCerts: HTMLElement,
    numSystemCerts: HTMLElement,
    // </if>
  };
}

export class LocalCertsSectionElement extends CrLitElement {
  static get is() {
    return 'local-certs-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      numPolicyCertsString_: {type: String},
      numUserCertsString_: {type: String},
      certManagementMetadata_: {type: Object},
      // <if expr="not is_chromeos">
      numSystemCertsString_: {type: String},
      // </if>
    };
  }

  protected accessor numPolicyCertsString_: string = '';
  protected accessor numUserCertsString_: string = '';
  protected accessor certManagementMetadata_: CertManagementMetadata|null =
      null;
  // <if expr="not is_chromeos">
  protected accessor numSystemCertsString_: string = '';
  // </if>

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.onMetadataRefresh_();
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.callbackRouter.triggerMetadataUpdate.addListener(
        this.onMetadataRefresh_.bind(this));
  }

  private onMetadataRefresh_() {
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.handler.getCertManagementMetadata().then(
        (results: {metadata: CertManagementMetadata}) => {
          this.certManagementMetadata_ = results.metadata;
          this.updateNumCertsStrings_();
        });
  }

  setFocusToLinkRow(p: Page) {
    switch (p) {
      case Page.ADMIN_CERTS:
        const adminLinkRow = this.shadowRoot.querySelector<HTMLElement>(
            '#adminCertsInstalledLinkRow');
        assert(adminLinkRow);
        focusWithoutInk(adminLinkRow);
        break;
      // <if expr="not is_chromeos">
      case Page.PLATFORM_CERTS:
        focusWithoutInk(this.$.viewOsImportedCerts);
        break;
      // </if>
      case Page.USER_CERTS:
        const userLinkRow = this.shadowRoot.querySelector<HTMLElement>(
            '#userCertsInstalledLinkRow');
        assert(userLinkRow);
        focusWithoutInk(userLinkRow);
        break;
      default:
        assertNotReached();
    }
  }

  private updateNumCertsStrings_() {
    if (!this.certManagementMetadata_) {
      this.numPolicyCertsString_ = '';
      // <if expr="not is_chromeos">
      this.numSystemCertsString_ = '';
      // </if>
      this.numUserCertsString_ = '';
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numPolicyCerts)
          .then(label => {
            this.numPolicyCertsString_ = label;
          });
      // <if expr="not is_chromeos">
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numUserAddedSystemCerts)
          .then(label => {
            this.numSystemCertsString_ = label;
          });
      // </if>
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numUserCerts)
          .then(label => {
            this.numUserCertsString_ = label;
          });
    }
  }

  // <if expr="not is_chromeos">
  protected onPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.PLATFORM_CERTS);
  }
  // </if>

  protected onAdminCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.ADMIN_CERTS);
  }

  protected onUserCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.USER_CERTS);
  }

  // <if expr="not is_chromeos">
  protected importOsCertsEnabled_(): boolean {
    return !!this.certManagementMetadata_?.includeSystemTrustStore;
  }

  protected importOsCertsEnabledManaged_(): boolean {
    return !!this.certManagementMetadata_?.isIncludeSystemTrustStoreManaged;
  }

  protected showViewOsCertsLinkRow_(): boolean {
    return !!this.certManagementMetadata_ &&
        this.certManagementMetadata_.numUserAddedSystemCerts > 0;
  }
  // </if>

  // If true, show the Custom Certs section.
  protected showCustomSection_(): boolean {
    return this.showPolicySection_() || this.showUserSection_();
  }

  // If true, show the Policy Certs section.
  protected showPolicySection_(): boolean {
    return !!this.certManagementMetadata_ &&
        this.certManagementMetadata_.numPolicyCerts > 0;
  }

  // If true, show the User Certs section.
  protected showUserSection_(): boolean {
    return !!this.certManagementMetadata_ &&
        this.certManagementMetadata_.showUserCertsUi;
  }

  // <if expr="is_win or is_macosx">
  protected onManageCertsExternalClick_() {
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.handler.showNativeManageCertificates();
  }
  // </if>

  // <if expr="not is_chromeos">
  protected onOsCertsToggleChange_(e: CustomEvent<boolean>) {
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.handler.setIncludeSystemTrustStore(e.detail);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'local-certs-section': LocalCertsSectionElement;
  }
}

customElements.define(LocalCertsSectionElement.is, LocalCertsSectionElement);
