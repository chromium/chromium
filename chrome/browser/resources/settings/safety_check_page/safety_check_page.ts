// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../settings_shared.css.js';
import './safety_check_extensions_child.js';
import './safety_check_passwords_child.js';
import './safety_check_safe_browsing_child.js';
import './safety_check_updates_child.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {NotificationPermission, SafetyHubBrowserProxy, UnusedSitePermissions} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';

import type {SafetyCheckBrowserProxy} from './safety_check_browser_proxy.js';
import {SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckParentStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckExtensionsBrowserProxyImpl} from './safety_check_extensions_browser_proxy.js';
import {getTemplate} from './safety_check_page.html.js';

interface ParentChangedEvent {
  newState: SafetyCheckParentStatus;
  displayString: string;
}

const SettingsSafetyCheckPageElementBase =
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsSafetyCheckPageElement extends
    SettingsSafetyCheckPageElementBase {
  static get is() {
    return 'settings-safety-check-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Current state of the safety check parent element. */
      parentStatus_: {
        type: Number,
        value: SafetyCheckParentStatus.BEFORE,
      },

      /** UI string to display for the parent status. */
      parentDisplayString_: String,

      /** Boolean to show/hide entry point for unused site permissions. */
      safetyCheckUnusedSitePermissionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'safetyCheckUnusedSitePermissionsEnabled');
        },
      },

      /** Boolean to show/hide extensions entry point. */
      safetyCheckExtensionsReviewEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('safetyCheckExtensionsReviewEnabled');
        },
      },

      /* List of notification permission sites. */
      notificationPermissionSites_: Array,
    };
  }

  private parentStatus_: SafetyCheckParentStatus;
  private parentDisplayString_: string;
  private safetyCheckUnusedSitePermissionsEnabled_: boolean;
  private safetyCheckExtensionsReviewEnabled_: boolean;
  private safetyCheckNumberOfExtensionsThatNeedReview_: number;
  private notificationPermissionSites_: NotificationPermission[] = [];
  private unusedSitePermissions_: UnusedSitePermissions[] = [];
  private shouldRecordMetrics_: boolean = false;
  private permissionsBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private safetyCheckBrowserProxy_: SafetyCheckBrowserProxy =
      SafetyCheckBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  /** Timer ID for periodic update. */
  private updateTimerId_: number = -1;

  override async connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUiListener(
        SafetyCheckCallbackConstants.PARENT_CHANGED,
        this.onSafetyCheckParentChanged_.bind(this));

    // Register for safety check status updates.
    this.addWebUiListener(
        SafetyCheckCallbackConstants.EXTENSIONS_CHANGED,
        this.shouldShowSafetyCheckExtensionsReview_.bind(this));

    // Configure default UI.
    this.parentDisplayString_ =
        this.i18n('safetyCheckParentPrimaryLabelBefore');

    if (Router.getInstance().getCurrentRoute() === routes.SAFETY_CHECK &&
        Router.getInstance().getQueryParameters().has('activateSafetyCheck')) {
      this.runSafetyCheck_();
    }

    // Register for notification permission review list updates.
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.notificationPermissionSites_ =
        await this.permissionsBrowserProxy_.getNotificationPermissionReview();

    // Register for updates on the unused site permission list.
    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.unusedSitePermissions_ = await this.permissionsBrowserProxy_
                                      .getRevokedUnusedSitePermissionsList();

    this.safetyCheckNumberOfExtensionsThatNeedReview_ =
        await SafetyCheckExtensionsBrowserProxyImpl.getInstance()
            .getNumberOfExtensionsThatNeedReview();

    if (this.shouldRecordMetrics_) {
      this.metricsBrowserProxy_
          .recordSafetyCheckNotificationsModuleEntryPointShown(
              this.shouldShowNotificationPermissions_());
      this.metricsBrowserProxy_
          .recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown(
              this.shouldShowUnusedSitePermissions_());
      this.shouldRecordMetrics_ = false;
    }
  }

  /**
   * Determine whether metrics should be recorded, namely only when the user
   * visited the privacy setting page embedding Safety Check, and not when
   * the element is rendered in the background.
   */
  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute.path === routes.PRIVACY.path) {
      this.shouldRecordMetrics_ = true;
    }
  }

  /** Triggers the safety check. */
  private runSafetyCheck_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.RUN_SAFETY_CHECK);
    this.metricsBrowserProxy_.recordAction('Settings.SafetyCheck.Start');

    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
    // Readout new safety check status via accessibility.
    getAnnouncerInstance().announce(this.i18n('safetyCheckAriaLiveRunning'));
  }

  private onSafetyCheckParentChanged_(event: ParentChangedEvent) {
    this.parentStatus_ = event.newState;
    this.parentDisplayString_ = event.displayString;
    if (this.parentStatus_ === SafetyCheckParentStatus.CHECKING) {
      // Ensure the re-run button is visible and focus it.
      flush();
      this.focusIconButton_();
    } else if (this.parentStatus_ === SafetyCheckParentStatus.AFTER) {
      // Start periodic safety check parent ran string updates.
      const update = async () => {
        this.parentDisplayString_ =
            await this.safetyCheckBrowserProxy_.getParentRanDisplayString();
      };
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = window.setInterval(update, 60000);
      // Run initial safety check parent ran string update now.
      update();
      // Readout new safety check status via accessibility.
      getAnnouncerInstance().announce(this.i18n('safetyCheckAriaLiveAfter'));
    }
  }

  private shouldShowParentButton_(): boolean {
    return this.parentStatus_ === SafetyCheckParentStatus.BEFORE;
  }

  private shouldShowParentIconButton_(): boolean {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  }

  private onRunSafetyCheckClick_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.RAN_SAFETY_CHECK);

    this.runSafetyCheck_();
  }

  private focusIconButton_() {
    this.shadowRoot!.querySelector('cr-icon-button')!.focus();
  }

  private shouldShowChildren_(): boolean {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  }

  private onReviewNotificationPermissionListChanged_(
      sites: NotificationPermission[]) {
    this.notificationPermissionSites_ = sites;
  }

  private shouldShowNotificationPermissions_(): boolean {
    return this.notificationPermissionSites_.length !== 0;
  }

  private onUnusedSitePermissionListChanged_(sites: UnusedSitePermissions[]) {
    this.unusedSitePermissions_ = sites;
  }

  private shouldShowUnusedSitePermissions_(): boolean {
    return this.safetyCheckUnusedSitePermissionsEnabled_ &&
        this.unusedSitePermissions_.length !== 0;
  }

  private shouldShowSafetyCheckExtensionsReview_(): boolean {
    if (this.safetyCheckExtensionsReviewEnabled_ &&
        this.safetyCheckNumberOfExtensionsThatNeedReview_ !== 0) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.SafetyCheck.ShownExtensionsReviewRow');
      return true;
    }
    return false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-page': SettingsSafetyCheckPageElement;
  }
}

customElements.define(
    SettingsSafetyCheckPageElement.is, SettingsSafetyCheckPageElement);
