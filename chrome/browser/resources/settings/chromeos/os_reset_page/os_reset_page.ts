// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the OS settings page containing reset
 * settings.
 */
import './os_powerwash_dialog.js';

import {getEuicc, getNonPendingESimProfiles} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './os_reset_page.html.js';

interface OsSettingsResetPageElement {
  $: {
    powerwash: CrButtonElement,
  };
}

const OsSettingsResetPageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

class OsSettingsResetPageElement extends OsSettingsResetPageElementBase {
  static get is() {
    return 'os-settings-reset-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showPowerwashDialog_: Boolean,

      installedESimProfiles_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kPowerwash]),
      },
    };
  }

  private installedESimProfiles_: ESimProfileRemote[];
  private showPowerwashDialog_: boolean;

  private onShowPowerwashDialog_(e: Event) {
    e.preventDefault();

    getEuicc().then(euicc => {
      if (!euicc) {
        this.installedESimProfiles_ = [];
        this.showPowerwashDialog_ = true;
        return;
      }
      getNonPendingESimProfiles(euicc).then(profiles => {
        this.installedESimProfiles_ = profiles;
        this.showPowerwashDialog_ = true;
      });
    });
  }

  private onPowerwashDialogClose_() {
    this.showPowerwashDialog_ = false;
    focusWithoutInk(this.$.powerwash);
  }

  override currentRouteChanged(newRoute: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (newRoute !== routes.OS_RESET) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-reset-page': OsSettingsResetPageElement;
  }
}

customElements.define(
    OsSettingsResetPageElement.is, OsSettingsResetPageElement);
