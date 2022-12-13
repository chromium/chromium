// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'bruschetta-subpage' is the settings subpage for managing Bruschetta
 * (third-party VMs).
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './bruschetta_subpage.html.js';

const BruschettaSubpageElementBase = RouteOriginMixin(PolymerElement);

class BruschettaSubpageElement extends BruschettaSubpageElementBase {
  static get is() {
    return 'settings-bruschetta-subpage';
  }

  static get template() {
    return getTemplate();
  }

  private route_: Route;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.BRUSCHETTA_DETAILS;
  }

  override ready() {
    super.ready();
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschetta-shared-usb-devices');
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_PATHS, '#bruschetta-shared-paths');
  }

  private onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }

  private onSharedPathsClick_() {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_PATHS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-bruschetta-subpage': BruschettaSubpageElement;
  }
}

customElements.define(BruschettaSubpageElement.is, BruschettaSubpageElement);
