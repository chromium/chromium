// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the details of a Passpoint
 * subscription.
 */

import '../settings_shared.css.js';

import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointServiceInterface, PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {App, AppType, PageHandlerInterface} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy as AppManagementComponentBrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrosNetworkConfigInterface, FilterType, NetworkCertificate, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {PasspointListenerMixin} from './passpoint_listener_mixin.js';
import {getTemplate} from './passpoint_subpage.html.js';

export class SettingsPasspointSubpageElement extends PasspointListenerMixin
(RouteObserverMixin(I18nMixin(PolymerElement))) {
  static get is() {
    return 'settings-passpoint-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The identifier of the subscription for which details are shown. */
      id_: String,

      /** Passpoint subscription currently displayed. */
      subscription_: Object,

      /** ARC application that provided the subscription. */
      app_: Object,

      /** List of Certificate Authorities available. */
      certs_: Array,

      /** Certificate authority common name. */
      certificateAuthorityName_: {
        type: String,
        computed: 'getCertificateAuthorityName_(certs_)',
      },

      /** Name of the provider of the subscription. */
      providerName_: {
        type: String,
        computed: 'getProviderName_(subscription_, app_)',
      },

      /** List of networks populated with the subscription. */
      networks_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** Tell if the forget dialog should be displayed. */
      showForgetDialog_: Boolean,

      domainsExpanded_: Boolean,
    };
  }

  private app_: App|null;
  private appHandler_: PageHandlerInterface;
  private certs_: NetworkCertificate[];
  private certificateAuthorityName_: string;
  private domainsExpanded_: boolean;
  private id_: string;
  private networkConfig_: CrosNetworkConfigInterface;
  private networks_: NetworkStateProperties[];
  private passpointService_: PasspointServiceInterface;
  private providerName_: string;
  private showForgetDialog_: boolean;
  private subscription_: PasspointSubscription|null;

  constructor() {
    super();
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.passpointService_ =
        MojoConnectivityProvider.getInstance().getPasspointService();
    this.appHandler_ = AppManagementComponentBrowserProxy.getInstance().handler;
  }

  close(): void {
    // If the page is already closed, return early to avoid navigating backward
    // erroneously.
    if (!this.id_) {
      return;
    }

    this.id_ = '';
    Router.getInstance().navigateToPreviousRoute();
  }


  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSPOINT_DETAIL) {
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    const id = queryParams.get('id') || '';
    if (!id) {
      console.warn('No Passpoint subscription ID specified for page:' + route);
      this.close();
      return;
    }
    this.id_ = id;
    this.refresh_();
  }

  private async refresh_(): Promise<void> {
    const response =
        await this.passpointService_.getPasspointSubscription(this.id_);
    if (!response.result) {
      console.warn('No subscription found for id ' + this.id_);
      this.close();
      return;
    }
    this.subscription_ = response.result;
    this.refreshCertificates_();
    this.refreshApp_(this.subscription_);
    this.refreshNetworks_(this.subscription_);
  }

  private async refreshCertificates_(): Promise<void> {
    const certs = await this.networkConfig_.getNetworkCertificates();
    this.certs_ = certs.serverCas;
  }

  private async refreshApp_(subscription: PasspointSubscription):
      Promise<void> {
    const response = await this.appHandler_.getApps();
    for (const app of response.apps) {
      if (app.type === AppType.kArc &&
          app.publisherId === subscription.provisioningSource) {
        this.app_ = app;
        return;
      }
    }
  }

  private async refreshNetworks_(subscription: PasspointSubscription):
      Promise<void> {
    const filter = {
      filter: FilterType.kConfigured,
      limit: NO_LIMIT,
      networkType: NetworkType.kWiFi,
    };
    const response = await this.networkConfig_.getNetworkStateList(filter);
    this.networks_ = response.result.filter(network => {
      return network.typeState!.wifi!.passpointId === subscription.id;
    });
  }

  private getCertificateAuthorityName_(): string {
    for (const cert of this.certs_) {
      if (cert.pemOrId === this.subscription_!.trustedCa) {
        return cert.issuedTo;
      }
    }
    return this.i18n('passpointSystemCALabel');
  }

  private hasExpirationDate_(): boolean {
    return this.subscription_!.expirationEpochMs > 0n;
  }

  private getExpirationDate_(subscription: PasspointSubscription): string {
    const date = new Date(Number(subscription.expirationEpochMs));
    return date.toLocaleDateString();
  }

  private getProviderName_(): string {
    if (this.app_ && this.app_.title !== undefined) {
      return this.app_.title!;
    }
    return this.subscription_!.provisioningSource;
  }

  private getPasspointDomainsList_(): string[] {
    return this.subscription_!.domains;
  }

  private getNetworkDisplayName_(networkState: OncMojo.NetworkStateProperties):
      string {
    return OncMojo.getNetworkStateDisplayNameUnsafe(networkState);
  }

  private hasNetworks_(): boolean {
    return this.networks_.length > 0;
  }

  private onAssociatedNetworkClicked_(
      event: DomRepeatEvent<OncMojo.NetworkStateProperties>): void {
    const networkState = event.model.item;
    const showDetailEvent = new CustomEvent(
        'show-detail', {bubbles: true, composed: true, detail: networkState});
    this.dispatchEvent(showDetailEvent);
    event.stopPropagation();
  }

  private getRemovalDialogDescription_(): TrustedHTML {
    return this.i18nAdvanced('passpointRemovalDescription', {
      substitutions: [
        this.subscription_!.friendlyName,
      ],
    });
  }

  private getRemovalDialog_(): HTMLDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<HTMLDialogElement>('#removalDialog'));
  }

  private onForgetClick_(): void {
    this.showForgetDialog_ = true;
  }

  private async onRemovalDialogConfirm_(): Promise<void> {
    this.showForgetDialog_ = false;
    const response =
        await this.passpointService_.deletePasspointSubscription(this.id_);
    if (response.success) {
      this.close();
      return;
    }
  }

  private onRemovalDialogCancel_(): void {
    this.showForgetDialog_ = false;
  }

  override onPasspointSubscriptionRemoved(subscription: PasspointSubscription):
      void {
    if (this.id_ === subscription.id) {
      // The subscription was removed, leave the page.
      this.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasspointSubpageElement.is]: SettingsPasspointSubpageElement;
  }
}

customElements.define(
    SettingsPasspointSubpageElement.is, SettingsPasspointSubpageElement);
