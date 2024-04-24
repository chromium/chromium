// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../pin_to_shelf_item.js';
import '../app_management_cros_shared_style.css.js';
import '../../../app_management_icons.html.js';
import '../permission_item.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../../common/app_management/store_mixin.js';
import {PrefsState} from '../../../common/types.js';
import {Router, routes} from '../../../router.js';

import {getTemplate} from './borealis_detail_view.html.js';

const BOREALIS_CLIENT_APP_ID = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

const AppManagementBorealisDetailViewElementBase =
    AppManagementStoreMixin(PolymerElement);

export class AppManagementBorealisDetailViewElement extends
    AppManagementBorealisDetailViewElementBase {
  static get is() {
    return 'app-management-borealis-detail-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      app_: {
        type: Object,
      },
    };
  }

  prefs: PrefsState;
  private app_: App;

  override connectedCallback(): void {
    super.connectedCallback();

    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }

  private isMainApp_(): boolean {
    return this.app_.id === BOREALIS_CLIENT_APP_ID;
  }

  private onBorealisLinkClicked_(event: CustomEvent<{event: Event}>): void {
    event.detail.event.preventDefault();
    const params = new URLSearchParams();
    params.append('id', BOREALIS_CLIENT_APP_ID);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-borealis-detail-view':
        AppManagementBorealisDetailViewElement;
  }
}

customElements.define(
    AppManagementBorealisDetailViewElement.is,
    AppManagementBorealisDetailViewElement);
