// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'bruschetta-subpage' is the settings subpage for managing Bruschetta
 * (third-party VMs).
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './bruschetta_subpage.html.js';
import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

const BruschettaSubpageElementBase =
    RouteOriginMixin(PrefsMixin(PolymerElement));

class BruschettaSubpageElement extends BruschettaSubpageElementBase {
  static get is() {
    return 'settings-bruschetta-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get observers() {
    return [
      'onInstalledChanged_(prefs.bruschetta.installed.value)',
    ];
  }

  private route_: Route;
  private browserProxy_: CrostiniBrowserProxy;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.BRUSCHETTA_DETAILS;

    // For now we reuse the Crostini browser proxy, we're both part of
    // crostini_page. At some point we may want to split them apart (or make
    // something for all guest OSs).
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschetta-shared-usb-devices');
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_PATHS, '#bruschetta-shared-paths');
  }

  private onSharedUsbDevicesClick_(): void {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }

  private onSharedPathsClick_(): void {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_PATHS);
  }

  private onRemoveClick_(): void {
    this.browserProxy_.requestBruschettaUninstallerView();
  }

  private onInstalledChanged_(installed: boolean): void {
    if (!installed &&
        Router.getInstance().currentRoute === routes.BRUSCHETTA_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-bruschetta-subpage': BruschettaSubpageElement;
  }
}

customElements.define(BruschettaSubpageElement.is, BruschettaSubpageElement);
