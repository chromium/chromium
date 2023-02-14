// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-subpage' is the settings subpage for managing android apps.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from './android_apps_browser_proxy.js';
import {getTemplate} from './android_apps_subpage.html.js';

interface SettingsAndroidAppsSubpageElement {
  $: {
    confirmDisableDialog: CrDialogElement,
  };
}

const SettingsAndroidAppsSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PrefsMixin(I18nMixin(PolymerElement))));

class SettingsAndroidAppsSubpageElement extends
    SettingsAndroidAppsSubpageElementBase {
  static get is() {
    return 'settings-android-apps-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      androidAppsInfo: {
        type: Object,
      },

      playStoreEnabled_: {
        type: Boolean,
        computed: 'computePlayStoreEnabled_(androidAppsInfo)',
        observer: 'onPlayStoreEnabledChanged_',
      },

      dialogBody_: {
        type: String,
        value(this: SettingsAndroidAppsSubpageElement): string {
          return this
              .i18nAdvanced(
                  'androidAppsDisableDialogMessage',
                  {substitutions: [], tags: ['br']})
              .toString();
        },
      },

      /** Whether Arc VM manage usb subpage should be shown. */
      showArcvmManageUsb: Boolean,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kManageAndroidPreferences,
          Setting.kRemovePlayStore,
        ]),
      },
    };
  }

  androidAppsInfo: AndroidAppsInfo;
  showArcvmManageUsb: boolean;
  private dialogBody_: string;
  private playStoreEnabled_: boolean;

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.ANDROID_APPS_DETAILS) {
      return;
    }

    this.attemptDeepLink();
  }

  private onPlayStoreEnabledChanged_(enabled: boolean) {
    if (!enabled &&
        Router.getInstance().currentRoute === routes.ANDROID_APPS_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  private computePlayStoreEnabled_(): boolean {
    return this.androidAppsInfo.playStoreEnabled;
  }

  private allowRemove_(): boolean {
    return this.prefs.arc.enabled.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  /**
   * Shows a confirmation dialog when disabling android apps.
   */
  private onRemoveTap_(): void {
    this.$.confirmDisableDialog.showModal();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   */
  private onConfirmDisableDialogConfirm_(): void {
    this.setPrefValue('arc.enabled', false);
    this.$.confirmDisableDialog.close();
    // Sub-page will be closed in onAndroidAppsInfoUpdate_ call.
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   */
  private onConfirmDisableDialogCancel_(): void {
    this.$.confirmDisableDialog.close();
  }

  private onConfirmDisableDialogClose_(): void {
    focusWithoutInk(castExists(this.shadowRoot!.querySelector('#remove')));
  }

  private onManageAndroidAppsTap_(event: MouseEvent): void {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail === 0;
    AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  }

  private onSharedUsbDevicesClick_(): void {
    Router.getInstance().navigateTo(
        routes.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsAndroidAppsSubpageElement.is]: SettingsAndroidAppsSubpageElement;
  }
}

customElements.define(
    SettingsAndroidAppsSubpageElement.is, SettingsAndroidAppsSubpageElement);
