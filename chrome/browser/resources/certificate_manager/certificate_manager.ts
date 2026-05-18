// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import '/strings.m.js';
import './certificate_list.js';
import './certificate_confirmation_dialog.js';
import './certificate_info_dialog.js';
import './certificate_password_dialog.js';
import './certificate_subpage.js';
import './certificate_manager_icons.html.js';
import './crs_section.js';
import './local_certs_section.js';
// <if expr="is_chromeos">
import './certificate_provisioning_list.js';
// </if>
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrPageSelectorElement} from '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateConfirmationDialogElement} from './certificate_confirmation_dialog.js';
import type {CertificateListElement} from './certificate_list.js';
import {getCss} from './certificate_manager.css.js';
import {getHtml} from './certificate_manager.html.js';
import type {ActionResult, SummaryCertInfo} from './certificate_manager.mojom-webui.js';
import {CertificateSource} from './certificate_manager.mojom-webui.js';
import type {CertificatePasswordDialogElement} from './certificate_password_dialog.js';
import type {CertificateSubpageElement, SubpageCertificateList} from './certificate_subpage.js';
import {CertificatesBrowserProxy} from './certificates_browser_proxy.js';
import type {CrsSectionElement} from './crs_section.js';
import type {LocalCertsSectionElement} from './local_certs_section.js';
import type {Route} from './navigation.js';
import {Page, RouteObserverMixin, Router} from './navigation.js';

interface PasswordResult {
  password: string|null;
}

interface ConfirmationResult {
  confirmed: boolean;
}

const CertificateManagerElementBase = RouteObserverMixin(CrLitElement);

export interface CertificateManagerElement {
  $: {
    toolbar: HTMLElement,
    main: CrPageSelectorElement,
    // <if expr="is_win or is_macosx or is_linux">
    provisionedClientCerts: CertificateListElement,
    // </if>
    // <if expr="is_chromeos">
    extensionsClientCerts: CertificateListElement,
    // </if>
    toast: CrToastElement,
    viewOsImportedClientCerts: HTMLElement,
    // <if expr="is_win or is_macosx">
    manageOsImportedClientCerts: HTMLElement,
    // </if>

    localMenuItem: HTMLElement,
    clientMenuItem: HTMLElement,
    crsMenuItem: HTMLElement,

    localCertSection: LocalCertsSectionElement,
    clientCertSection: HTMLElement,
    crsCertSection: CrsSectionElement,
    adminCertsSection: CertificateSubpageElement,
    userCertsSection: CertificateSubpageElement,
    // <if expr="not is_chromeos">
    platformCertsSection: CertificateSubpageElement,
    // </if>
    platformClientCertsSection: CertificateSubpageElement,
    scrollableShadow: HTMLElement,
  };
}

export class CertificateManagerElement extends CertificateManagerElementBase {
  static get is() {
    return 'certificate-manager';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedPage_: {type: String},
      clientPlatformSubpageLists_: {type: Array},
      enterpriseSubpageLists_: {type: Array},
      // <if expr="not is_chromeos">
      platformSubpageLists_: {type: Array},
      // </if>

      userSubpageLists_: {type: Array},

      toastMessage_: {type: String},

      showInfoDialog_: {type: Boolean},

      infoDialogTitle_: {type: String},
      infoDialogMessage_: {type: String},

      showPasswordDialog_: {type: Boolean},

      confirmationDialogTitle_: {type: String},
      confirmationDialogMessage_: {type: String},

      showConfirmationDialog_: {type: Boolean},

      showSearch_: {type: Boolean},

      numPlatformClientCertsString_: {type: String},
    };
  }

  protected accessor selectedPage_: Page = Page.LOCAL_CERTS;
  protected accessor userSubpageLists_: SubpageCertificateList[] = [
    {
      headerText:
          loadTimeData.getString('certificateManagerV2TrustedCertsList'),
      certSource: CertificateSource.kUserTrustedCerts,
      showImport: loadTimeData.getBoolean('userCertsImportAllowed'),
      certMetadataEditable: loadTimeData.getBoolean('userCertsImportAllowed'),
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2IntermediateCertsList'),
      certSource: CertificateSource.kUserIntermediateCerts,
      showImport: loadTimeData.getBoolean('userCertsImportAllowed'),
      certMetadataEditable: loadTimeData.getBoolean('userCertsImportAllowed'),
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2DistrustedCertsList'),
      certSource: CertificateSource.kUserDistrustedCerts,
      showImport: loadTimeData.getBoolean('userCertsImportAllowed'),
      certMetadataEditable: loadTimeData.getBoolean('userCertsImportAllowed'),
    },
  ];

  protected accessor toastMessage_: string = '';
  protected accessor showInfoDialog_: boolean = false;
  protected accessor infoDialogTitle_: string = '';
  protected accessor infoDialogMessage_: string = '';
  protected accessor showPasswordDialog_: boolean = false;
  protected accessor showConfirmationDialog_: boolean = false;
  protected accessor showSearch_: boolean = false;
  protected accessor confirmationDialogTitle_: string = '';
  protected accessor confirmationDialogMessage_: string = '';
  protected accessor clientPlatformSubpageLists_: SubpageCertificateList[] = [
    {
      headerText:
          loadTimeData.getString('certificateManagerV2ClientCertsFromPlatform'),
      certSource: CertificateSource.kPlatformClientCert,
      hideExport: true,
      // <if expr="is_chromeos">
      showImport: loadTimeData.getBoolean('clientCertImportAllowed'),
      showImportAndBind: loadTimeData.getBoolean(
          'clientCertImportAndBindAllowed'),
      // TODO(crbug.com/40928765): Figure out how we want to display the
      // import buttons/etc on this subpage. For now just show the header
      // when we need the import buttons to be visible.
      hideHeader: !loadTimeData.getBoolean('clientCertImportAllowed') &&
          !loadTimeData.getBoolean('clientCertImportAndBindAllowed'),
      // </if>
      // <if expr="is_linux">
      showImport: true,
      hideHeader: false,
      // </if>
      // <if expr="not is_chromeos and not is_linux">
      hideHeader: true,
      // </if>
    },
  ];
  protected accessor enterpriseSubpageLists_: SubpageCertificateList[] = [
    {
      headerText:
          loadTimeData.getString('certificateManagerV2TrustedCertsList'),
      certSource: CertificateSource.kEnterpriseTrustedCerts,
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2IntermediateCertsList'),
      certSource: CertificateSource.kEnterpriseIntermediateCerts,
      hideIfEmpty: true,
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2DistrustedCertsList'),
      certSource: CertificateSource.kEnterpriseDistrustedCerts,
      hideIfEmpty: true,
    },
  ];

  // <if expr="not is_chromeos">
  protected accessor platformSubpageLists_: SubpageCertificateList[] = [
    {
      headerText:
          loadTimeData.getString('certificateManagerV2TrustedCertsList'),
      certSource: CertificateSource.kPlatformUserTrustedCerts,
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2IntermediateCertsList'),
      certSource: CertificateSource.kPlatformUserIntermediateCerts,
      hideIfEmpty: true,
    },
    {
      headerText:
          loadTimeData.getString('certificateManagerV2DistrustedCertsList'),
      certSource: CertificateSource.kPlatformUserDistrustedCerts,
      hideIfEmpty: true,
    },
  ];
  // </if>
  protected accessor numPlatformClientCertsString_: string = '';

  private passwordEntryResolver_: PromiseResolver<PasswordResult>|null = null;
  private confirmationDialogResolver_: PromiseResolver<ConfirmationResult>|
      null = null;

  override connectedCallback() {
    super.connectedCallback();
    const enableWebuiRefresh2026 =
        loadTimeData.getString('webuiRefresh2026') !== '';
    if (enableWebuiRefresh2026) {
      this.addThemedColors_();
      ColorChangeUpdater.forDocument().start();
    }
  }

  override firstUpdated() {
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.callbackRouter.askForImportPassword.addListener(
        this.onAskForImportPassword_.bind(this));
    proxy.callbackRouter.askForConfirmation.addListener(
        this.onAskForConfirmation_.bind(this));
    proxy.callbackRouter.triggerReload.addListener(
        this.onTriggerReload_.bind(this));
    this.getClientCertCount_();
  }

  private onTriggerReload_(certSources: CertificateSource[]) {
    if (certSources.includes(CertificateSource.kPlatformClientCert)) {
      this.getClientCertCount_();
    }
  }

  private getClientCertCount_() {
    CertificatesBrowserProxy.getInstance()
        .handler.getCertificates(CertificateSource.kPlatformClientCert)
        .then((results: {certs: SummaryCertInfo[]}) => {
          PluralStringProxyImpl.getInstance()
              .getPluralString(
                  'certificateManagerV2NumCerts', results.certs.length)
              .then(label => {
                this.numPlatformClientCertsString_ = label;
              });
        });
  }

  private onAskForImportPassword_(): Promise<PasswordResult> {
    this.showPasswordDialog_ = true;
    assert(this.passwordEntryResolver_ === null);
    this.passwordEntryResolver_ = new PromiseResolver<PasswordResult>();
    return this.passwordEntryResolver_.promise;
  }

  protected onPasswordDialogClose_() {
    const passwordDialog =
        this.shadowRoot.querySelector<CertificatePasswordDialogElement>(
            '#passwordDialog');
    assert(passwordDialog);
    assert(this.passwordEntryResolver_);
    this.passwordEntryResolver_.resolve({password: passwordDialog.value()});
    this.passwordEntryResolver_ = null;
    this.showPasswordDialog_ = false;
  }

  private onAskForConfirmation_(title: string, message: string):
      Promise<ConfirmationResult> {
    this.confirmationDialogTitle_ = title;
    this.confirmationDialogMessage_ = message;
    this.showConfirmationDialog_ = true;
    assert(this.confirmationDialogResolver_ === null);
    this.confirmationDialogResolver_ =
        new PromiseResolver<ConfirmationResult>();
    return this.confirmationDialogResolver_.promise;
  }

  protected onConfirmationDialogClose_() {
    const confirmationDialog =
        this.shadowRoot.querySelector<CertificateConfirmationDialogElement>(
            '#confirmationDialog');
    assert(confirmationDialog);
    assert(this.confirmationDialogResolver_);
    this.confirmationDialogResolver_.resolve(
        {confirmed: confirmationDialog.wasConfirmed()});
    this.confirmationDialogResolver_ = null;
    this.showConfirmationDialog_ = false;
  }

  protected onHashCopied_() {
    this.toastMessage_ =
        loadTimeData.getString('certificateManagerV2HashCopiedToast');
    this.$.toast.show();
  }

  // Prevent clicks on sidebar items from navigating and therefore reloading
  // the page.
  protected onMenuItemClick_(e: MouseEvent) {
    e.preventDefault();
  }

  override async currentRouteChanged(route: Route, oldRoute: Route) {
    this.selectedPage_ = route.page;
    await this.updateComplete;
    this.$.scrollableShadow.classList.toggle('force-on', route.isSubpage());

    if (route.isSubpage()) {
      switch (route.page) {
        case Page.ADMIN_CERTS:
          this.$.adminCertsSection.setInitialFocus();
          break;
        // <if expr="not is_chromeos">
        case Page.PLATFORM_CERTS:
          this.$.platformCertsSection.setInitialFocus();
          break;
        // </if>
        case Page.PLATFORM_CLIENT_CERTS:
          this.$.platformClientCertsSection.setInitialFocus();
          break;
        case Page.USER_CERTS:
          this.$.userCertsSection.setInitialFocus();
          break;
        default:
          assertNotReached();
      }
    } else if (oldRoute.isSubpage()) {
      // If we're navigating back from a subpage, we may need to fiddle
      // with the focus element if we're going back to its parent page.
      switch (oldRoute.page) {
        case Page.ADMIN_CERTS:
          if (route.page === Page.LOCAL_CERTS) {
            this.$.localCertSection.setFocusToLinkRow(oldRoute.page);
          }
          break;
        // <if expr="not is_chromeos">
        case Page.PLATFORM_CERTS:
          if (route.page === Page.LOCAL_CERTS) {
            this.$.localCertSection.setFocusToLinkRow(oldRoute.page);
          }
          break;
        // </if>
        case Page.PLATFORM_CLIENT_CERTS:
          if (route.page === Page.CLIENT_CERTS) {
            focusWithoutInk(this.$.viewOsImportedClientCerts);
          }
          break;
        case Page.USER_CERTS:
          if (route.page === Page.LOCAL_CERTS) {
            this.$.localCertSection.setFocusToLinkRow(oldRoute.page);
          }
          break;
        default:
          assertNotReached();
      }
    }
  }

  protected onMenuIronActivate_(e: CustomEvent<{item: HTMLElement}>) {
    const page = e.detail.item.getAttribute('href');
    assert(page, 'Page is not available');
    Router.getInstance().navigateTo(page.substring(1) as Page);
  }

  protected getSelectedTopLevelHref_(): string {
    switch (this.selectedPage_) {
      // <if expr="not is_chromeos">
      case Page.PLATFORM_CERTS:
      // </if>
      case Page.ADMIN_CERTS:
      case Page.USER_CERTS:
        return this.generateHrefForPage_(Page.LOCAL_CERTS);
      case Page.PLATFORM_CLIENT_CERTS:
        return this.generateHrefForPage_(Page.CLIENT_CERTS);
      default:
        return this.generateHrefForPage_(this.selectedPage_);
    }
  }

  protected generateHrefForPage_(p: Page): string {
    return '/' + p;
  }


  protected onClientPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.PLATFORM_CLIENT_CERTS);
  }

  protected onImportResult_(e: CustomEvent<ActionResult|null>) {
    const result = e.detail;
    if (result === null) {
      return;
    }
    if (result.error !== undefined) {
      this.infoDialogTitle_ =
          loadTimeData.getString('certificateManagerV2ImportErrorTitle');
      this.infoDialogMessage_ = result.error;
      this.showInfoDialog_ = true;
    }
  }

  protected onDeleteResult_(e: CustomEvent<ActionResult|null>) {
    const result = e.detail;
    if (result === null) {
      return;
    }
    if (result.error !== undefined) {
      this.infoDialogTitle_ =
          loadTimeData.getString('certificateManagerV2DeleteErrorTitle');
      this.infoDialogMessage_ = result.error;
      this.showInfoDialog_ = true;
    }
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  // <if expr="is_win or is_macosx">
  protected onManageCertsExternalClick_() {
    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.handler.showNativeManageCertificates();
  }
  // </if>

  private addThemedColors_() {
    assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=ui,chrome';
    document.body.appendChild(link);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager': CertificateManagerElement;
  }
}

customElements.define(CertificateManagerElement.is, CertificateManagerElement);
