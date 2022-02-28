// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-subpage' is the settings subpage for managing android apps.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from './android_apps_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-android-apps-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: Object,

    /** @private {!AndroidAppsInfo|undefined} */
    androidAppsInfo: {
      type: Object,
    },

    /** @private */
    playStoreEnabled_: {
      type: Boolean,
      computed: 'computePlayStoreEnabled_(androidAppsInfo)',
      observer: 'onPlayStoreEnabledChanged_'
    },

    /** @private */
    dialogBody_: {
      type: String,
      value() {
        return this.i18nAdvanced(
            'androidAppsDisableDialogMessage',
            {substitutions: [], tags: ['br']});
      }
    },

    /** Whether Arc VM manage usb subpage should be shown. */
    showArcvmManageUsb: Boolean,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kManageAndroidPreferences,
        chromeos.settings.mojom.Setting.kRemovePlayStore,
      ]),
    },
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.ANDROID_APPS_DETAILS) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onPlayStoreEnabledChanged_(enabled) {
    if (!enabled &&
        Router.getInstance().getCurrentRoute() ===
            routes.ANDROID_APPS_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computePlayStoreEnabled_() {
    return this.androidAppsInfo.playStoreEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  allowRemove_() {
    return this.prefs.arc.enabled.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * Shows a confirmation dialog when disabling android apps.
   * @param {!Event} event
   * @private
   */
  onRemoveTap_(event) {
    this.$.confirmDisableDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onConfirmDisableDialogConfirm_() {
    this.setPrefValue('arc.enabled', false);
    this.$.confirmDisableDialog.close();
    // Sub-page will be closed in onAndroidAppsInfoUpdate_ call.
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onConfirmDisableDialogCancel_() {
    this.$.confirmDisableDialog.close();
  },

  /** @private */
  onConfirmDisableDialogClose_() {
    focusWithoutInk(assert(this.$$('#remove')));
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail === 0;
    AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(
        routes.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES);
  },
});
