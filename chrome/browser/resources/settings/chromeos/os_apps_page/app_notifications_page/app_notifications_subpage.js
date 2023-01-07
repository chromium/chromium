// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_notification_row.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/app-management/file_path.mojom-lite.js';
import '/app-management/image.mojom-lite.js';
import '/app-management/safe_base_name.mojom-lite.js';
import '/app-management/types.mojom-lite.js';
import '/os_apps_page/app_notification_handler.mojom-lite.js';

import {assert} from 'chrome://resources/js/assert.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../../deep_linking_behavior.js';
import {recordSettingChange} from '../../metrics_recorder.js';
import {routes} from '../../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../../route_observer_behavior.js';
import {isAppInstalled} from '../os_apps_page.js';

import {getAppNotificationProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'app-notifications-page' is responsible for containing controls for
 * notifications of all apps.
 * TODO(ethanimooney): Implement this skeleton element.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const AppNotificationsSubpageBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
export class AppNotificationsSubpage extends AppNotificationsSubpageBase {
  static get is() {
    return 'settings-app-notifications-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Reflects the Do Not Disturb property.
       * @private
       */
      isDndEnabled_: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },

      /**
       * @type {!Array<!Object>}
       * @private
       */
      appList_: {
        type: Array,
        value: [],
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kDoNotDisturbOnOff,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private */
    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    /**
     * Receiver responsible for observing app notification events.
     * @private {
     *    ?ash.settings.appNotification.mojom.
     *    AppNotificationsObserverReceiver
     * }
     */
    this.appNotificationsObserverReceiver_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.startObservingAppNotifications_();
    this.mojoInterfaceProvider_.getQuietMode().then((result) => {
      this.isDndEnabled_ = result.enabled;
    });
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appList_ = result.apps;
    });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.appNotificationsObserverReceiver_.$.close();
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== routes.APP_NOTIFICATIONS) {
      return;
    }
    this.attemptDeepLink();
  }

  /** @private */
  startObservingAppNotifications_() {
    this.appNotificationsObserverReceiver_ =
        new ash.settings.appNotification.mojom.AppNotificationsObserverReceiver(
            /**
             * @type {!ash.settings.appNotification.mojom.
             * AppNotificationsObserverInterface}
             */
            (this));
    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Override ash.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(enabled) {
    this.isDndEnabled_ = enabled;
  }

  /** Override ash.settings.appNotification.onNotificationAppChanged */
  onNotificationAppChanged(updatedApp) {
    // Using Polymer mutation methods do not properly handle splice updates with
    // object that have deep properties. Create and assign a copy list instead.
    const appList = Array.from(this.appList_);
    const foundIdx = this.appList_.findIndex(app => {
      return app.id === updatedApp.id;
    });
    if (isAppInstalled(updatedApp)) {
      if (foundIdx !== -1) {
        appList[foundIdx] = updatedApp;
      } else {
        appList.push(updatedApp);
      }
      this.appList_ = appList;
      return;
    }

    // Cannot have an app that is uninstalled prior to being installed.
    assert(foundIdx !== -1);
    // Uninstalled app found, remove it from the list.
    appList.splice(foundIdx, 1);
    this.appList_ = appList;
  }

  /** @private */
  setQuietMode_() {
    this.isDndEnabled_ = !this.isDndEnabled_;
    this.mojoInterfaceProvider_.setQuietMode(this.isDndEnabled_);
    recordSettingChange(
        Setting.kDoNotDisturbOnOff, {boolValue: this.isDndEnabled_});
  }

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    this.setQuietMode_();
    event.stopPropagation();
  }

  /**
   * A function used for sorting languages alphabetically.
   * @param {!Object} first An app array item.
   * @param {!Object} second An app array item.
   * @return {number} The result of the comparison.
   * @private
   */
  alphabeticalSort_(first, second) {
    return first.title.localeCompare(second.title);
  }
}


customElements.define(AppNotificationsSubpage.is, AppNotificationsSubpage);
