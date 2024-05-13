// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-notification-permissions-module' is the module in Safety
 * Hub page that show the origins sending a lot of notifications.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';
import '../icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {isUndoKeyboardEvent} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckNotificationsModuleInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {NotificationPermission, SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';
import {TooltipMixin} from '../tooltip_mixin.js';

import {getTemplate} from './notification_permissions_module.html.js';
import type {SettingsSafetyHubModuleElement, SiteInfo, SiteInfoWithTarget} from './safety_hub_module.js';

export interface SettingsSafetyHubNotificationPermissionsModuleElement {
  $: {
    actionMenu: CrActionMenuElement,
    blockAllButton: HTMLElement,
    bulkUndoButton: HTMLElement,
    headerActionMenu: CrActionMenuElement,
    ignore: HTMLElement,
    module: SettingsSafetyHubModuleElement,
    reset: HTMLElement,
    toastUndoButton: HTMLElement,
    undoNotification: HTMLElement,
    undoToast: CrToastElement,
  };
}

/**
 * The list of actions that a user can take with regards to the permissions of
 * notifications.
 */
enum Actions {
  BLOCK = 'block',
  BLOCK_ALL = 'block_all',
  IGNORE = 'ignore',
  RESET = 'reset',
}

/* Information about notification permissions. */
interface NotificationPermissionsDisplay extends NotificationPermission,
                                                 SiteInfo {}

const SettingsSafetyHubNotificationPermissionsModuleElementBase =
    TooltipMixin(WebUiListenerMixin(RouteObserverMixin(
        BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement))))));

export class SettingsSafetyHubNotificationPermissionsModuleElement extends
    SettingsSafetyHubNotificationPermissionsModuleElementBase {
  static get is() {
    return 'settings-safety-hub-notification-permissions-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The string for the primary header label.
      headerString_: String,

      // Text below primary header label.
      subheaderString_: String,

      // The icon next to primary header label.
      headerIconString_: String,

      // The text that will be shown in the undo toast element.
      toastText_: String,

      // The last action taken by the user: block, reset or ignore.
      lastUserAction_: String,

      // The last origins that the user interacted with.
      lastOrigins_: Array,

      // List of domains that sends a lot of notifications.
      sites_: {
        type: Array,
        value: null,
      },

      // Indicates whether user has finished the review process.
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed: 'computeShouldShowCompletionInfo_(sites_.*)',
      },
    };
  }

  static get observers() {
    return [
      'updateUndoNotificationText_(lastUserAction_, lastOrigins_)',
      'onSitesChanged_(sites_, shouldShowCompletionInfo_)',
    ];
  }

  private headerString_: string;
  private subheaderString_: string;
  private headerIconString_: string;
  private toastText_: string|null;
  private sites_: NotificationPermissionsDisplay[]|null;
  private shouldShowCompletionInfo_: boolean;
  private lastOrigins_: string[] = [];
  private renderedOrigins_: string[] = [];
  private lastUserAction_: Actions|null;
  private eventTracker_: EventTracker = new EventTracker();
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    // Register for review notification permission list updates.
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onNotificationPermissionListChanged_(sites));

    const sites = await this.browserProxy_.getNotificationPermissionReview();
    this.onNotificationPermissionListChanged_(sites);
    // This should be called after the sites have been retrieved such that
    // currentRouteChanged is called afterwards.
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.eventTracker_.removeAll();
  }

  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute !== routes.SAFETY_HUB) {
      // Remove event listener when navigating away from the page.
      this.eventTracker_.removeAll();
      return;
    }

    if (this.sites_ !== null) {
      this.metricsBrowserProxy_
          .recordSafetyHubNotificationPermissionsModuleListCountHistogram(
              this.sites_.length);
    }

    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
  }

  /* Repopulate the list when notification permission list is updated. */
  private onNotificationPermissionListChanged_(sites:
                                                   NotificationPermission[]) {
    this.sites_ = sites.map(
        (site: NotificationPermission): NotificationPermissionsDisplay =>
            ({...site, detail: site.notificationInfoString}));
  }

  private async setHeaderToCompletionState_() {
    assert(this.toastText_);
    this.headerString_ = this.toastText_!;
    this.subheaderString_ = '';
    this.headerIconString_ = 'cr:check';
  }

  private async onSitesChanged_() {
    if (this.sites_ === null) {
      return;
    }

    // Run the show animation on all new items, i.e. those items
    // in |this.sites_| which aren't already rendered.
    this.$.module.animateShow(
        this.sites_.map(site => site.origin)
            .filter(origin => !this.renderedOrigins_.includes(origin)));
    this.renderedOrigins_ = this.sites_.map(site => site.origin);

    if (this.shouldShowCompletionInfo_) {
      this.setHeaderToCompletionState_();
      return;
    }

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsPrimaryLabel', this.sites_.length);
    this.subheaderString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsSecondaryLabel',
            this.sites_.length);
    this.headerIconString_ = 'settings:notifications-none';
  }

  /** Clears all the changes made by a previous action. */
  private resetValues_(e: Event) {
    e.stopPropagation();
    this.$.undoToast.hide();
    this.lastOrigins_ = [];
    this.lastUserAction_ = null;
  }

  /** Sets all the values needed for an action. */
  private setValues_(origins: string[], action: Actions) {
    //  Both lastUserAction_ and lastOrigins_ need to be reset before setting
    //  new values to prevent triggering updateUndoNotificationText_ twice
    //  (which can cause issues like "flickering" on the header string, e.g.
    //  when the wrong header string is shown for a split second before the
    //  correct one).
    assert(!this.lastUserAction_);
    assert(!this.lastOrigins_.length);
    this.lastOrigins_ = origins;
    this.lastUserAction_ = action;
  }

  private onBlockClick_(e: CustomEvent<NotificationPermission>) {
    this.resetValues_(e);
    this.setValues_([e.detail.origin], Actions.BLOCK);
    this.showUndoToast_();

    this.$.module.animateHide(
        e.detail.origin,
        this.browserProxy_.blockNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.BLOCK);
  }

  private onMoreActionClick_(e: CustomEvent<SiteInfoWithTarget>) {
    this.resetValues_(e);
    this.lastOrigins_ = [e.detail.origin];
    this.$.actionMenu.showAt(e.detail.target as HTMLElement);
  }

  private onIgnoreClick_(e: Event) {
    const tempLastOrigins = this.lastOrigins_;
    this.resetValues_(e);
    this.setValues_(tempLastOrigins, Actions.IGNORE);
    this.showUndoToast_();
    this.$.actionMenu.close();

    // |lastOrigins| is set to a 1-item array containing the item on which
    // the context menu with the |reset| option was open,
    // in |onMoreActionClick_|.
    this.$.module.animateHide(
        this.lastOrigins_[0],
        this.browserProxy_.ignoreNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.IGNORE);
  }

  private onResetClick_(e: Event) {
    const tempLastOrigins = this.lastOrigins_;
    this.resetValues_(e);
    this.setValues_(tempLastOrigins, Actions.RESET);
    this.showUndoToast_();
    this.$.actionMenu.close();

    // |lastOrigins| is set to a 1-item array containing the item on which
    // the context menu with the |reset| option was open,
    // in |onMoreActionClick_|.
    this.$.module.animateHide(
        this.lastOrigins_[0],
        this.browserProxy_.resetNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.RESET);
  }

  private onBlockAllClick_(e: Event) {
    this.resetValues_(e);
    // To be able to undo the block-all action, we need to keep track of all
    // origins that were blocked.
    assert(this.sites_);
    this.setValues_(this.sites_.map(site => site.origin), Actions.BLOCK_ALL);

    this.$.module.animateHide(
        /* all origins */ null,
        this.browserProxy_.blockNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.BLOCK_ALL);
  }

  private onUndoClick_(e: Event) {
    e.stopPropagation();
    this.undoLastAction_();
  }

  private onHeaderMoreActionClick_(e: Event) {
    e.stopPropagation();
    this.$.headerActionMenu.showAt(e.target as HTMLElement);
  }

  private onGoToSettingsClick_(e: Event) {
    e.stopPropagation();
    this.$.headerActionMenu.close();
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_NOTIFICATIONS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);

    this.metricsBrowserProxy_
        .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.GO_TO_SETTINGS);
  }

  private async updateUndoNotificationText_() {
    if (!this.lastUserAction_ || this.lastOrigins_.length === 0) {
      return;
    }
    switch (this.lastUserAction_) {
      case Actions.BLOCK:
        this.toastText_ = this.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            this.lastOrigins_[0]);
        break;
      case Actions.BLOCK_ALL:
        this.toastText_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'safetyCheckNotificationPermissionReviewBlockAllToastLabel',
                this.lastOrigins_.length);
        break;
      case Actions.IGNORE:
        this.toastText_ = this.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            this.lastOrigins_[0]);
        break;
      case Actions.RESET:
        this.toastText_ = this.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            this.lastOrigins_[0]);
        break;
      default:
        assertNotReached();
    }
  }

  private showUndoToast_() {
    // Only show Undo toast if there are multiple sites to review. Otherwise,
    // once the single site is reviewed, the completion state with a permanent
    // Undo button in the header will be shown.
    if (this.sites_!.length > 1) {
      this.$.undoToast.show();
    }
  }

  private undoLastAction_() {
    switch (this.lastUserAction_) {
      // As BLOCK and RESET actions just change the notification permission,
      // undoing them only requires allowing notification permissions again.
      case Actions.BLOCK:
        this.browserProxy_.allowNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.metricsBrowserProxy_
            .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK);
        break;
      case Actions.BLOCK_ALL:
        this.browserProxy_.allowNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.metricsBrowserProxy_
            .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK_ALL);
        break;
      case Actions.RESET:
        this.browserProxy_.allowNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.metricsBrowserProxy_
            .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_RESET);
        break;
      case Actions.IGNORE:
        this.browserProxy_.undoIgnoreNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.metricsBrowserProxy_
            .recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_IGNORE);
        break;
      default:
        assertNotReached();
    }
    this.$.undoToast.hide();
  }

  private onKeyDown_(e: KeyboardEvent) {
    // Only allow undoing via ctrl+z when the undo toast is opened.
    if (!this.$.undoToast.open) {
      return;
    }

    if (isUndoKeyboardEvent(e)) {
      this.undoLastAction_();
      e.stopPropagation();
    }
  }

  /** Show info that review is completed when there are no permissions left. */
  private computeShouldShowCompletionInfo_(): boolean {
    return this.sites_ !== null && this.sites_.length === 0;
  }

  private getIgnoreAriaLabelForOrigins(origins: string[]): string {
    // A label is only needed when the action menu is shown for a single origin.
    if (origins.length !== 1) {
      return '';
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewIgnoreAriaLabel', origins[0]);
  }

  private getResetAriaLabelForOrigins(origins: string[]): string {
    // A label is only needed when the action menu is shown for a single origin.
    if (origins.length !== 1) {
      return '';
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewResetAriaLabel', origins[0]);
  }

  private showUndoTooltip_(e: Event) {
    e.stopPropagation();
    const tooltip = this.shadowRoot!.querySelector('cr-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target! as Element);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-notification-permissions-module':
        SettingsSafetyHubNotificationPermissionsModuleElement;
  }
}

customElements.define(
    SettingsSafetyHubNotificationPermissionsModuleElement.is,
    SettingsSafetyHubNotificationPermissionsModuleElement);
