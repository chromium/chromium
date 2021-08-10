// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_notification_row.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/app-management/file_path.mojom-lite.js';
import '/app-management/image.mojom-lite.js';
import '/app-management/types.mojom-lite.js';
import '/os_apps_page/app_notification_handler.mojom-lite.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, RouteObserverBehavior, RouteObserverBehaviorInterface, Router} from '../../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../../deep_linking_behavior.m.js';
import {routes} from '../../os_route.m.js';

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
        // TODO(ethanimooney): Replace placeholders with proper implementation
        // for apps
        value: [
          {title: 'Chrome', id: 'mgndgikekgjfcpckkfioiadnlibdjbkf'},
          {title: 'Files', id: 'hhaomjibdihmijegdhdafkllkbggdgoj'}
        ],
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kDoNotDisturbOnOff,
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
     *    ?chromeos.settings.appNotification.mojom.
     *    AppNotificationsObserverReceiver
     * }
     */
    this.appNotificationsObserverReceiver_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.startObservingAppNotifications_();
    this.mojoInterfaceProvider_.notifyPageReady();
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
        new chromeos.settings.appNotification.mojom
            .AppNotificationsObserverReceiver(
                /**
                 * @type {!chromeos.settings.appNotification.mojom.
                 * AppNotificationsObserverInterface}
                 */
                (this));
    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Override chromeos.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(enabled) {
    this.isDndEnabled_ = enabled;
  }

  /** @private */
  setQuietMode_() {
    this.mojoInterfaceProvider_.setQuietMode(this.isDndEnabled_);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    this.isDndEnabled_ = !this.isDndEnabled_;
    this.mojoInterfaceProvider_.setQuietMode(this.isDndEnabled_);
    event.stopPropagation();
  }
}


customElements.define(AppNotificationsSubpage.is, AppNotificationsSubpage);