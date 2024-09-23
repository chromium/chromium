// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../settings_shared.css.js';
import '../site_settings_page/site_review_shared.css.js';
import '../i18n_setup.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {isUndoKeyboardEvent} from 'chrome://resources/js/util.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckNotificationsModuleInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';
import type {NotificationPermission, SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import {MODEL_UPDATE_DELAY_MS} from '../site_settings/constants.js';
import {TooltipMixin} from '../tooltip_mixin.js';

import {getTemplate} from './review_notification_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface SettingsReviewNotificationPermissionsElement {
  $: {
    undoToast: CrToastElement,
    undoNotification: HTMLElement,
  };
}

/**
 * The list of actions that a user can take with regards to the permissions of
 * notifications.
 */
enum Actions {
  BLOCK = 'block',
  IGNORE = 'ignore',
  RESET = 'reset',
}

const SettingsReviewNotificationPermissionsElementBase =
    TooltipMixin(WebUiListenerMixin(RouteObserverMixin(
        BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement))))));

export class SettingsReviewNotificationPermissionsElement extends
    SettingsReviewNotificationPermissionsElementBase {
  static get is() {
    return 'review-notification-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* List of domains that sends a lot of notifications. */
      sites_: {
        type: Array,
        value: [],
        observer: 'onSitesChanged_',
      },

      /* If the list of notification permissions is expanded or collapsed. */
      notificationPermissionReviewListExpanded_: {
        type: Boolean,
        value: true,
        observer: 'updateNotificationPermissionReviewListExpanded_',
      },

      /* The last action taken by the user: block, reset or ignore. */
      lastUserAction_: {
        type: Actions,
        observer: 'updateUndoNotificationText_',
      },

      /* The last origins that the user interacted with. */
      lastOrigins_: {
        type: Array,
        observer: 'updateUndoNotificationText_',
      },

      /**
       * Indicates whether to show completion info after user has finished the
       * review process.
       */
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed: 'computeShouldShowCompletionInfo_(sites_.*)',
      },

      /* The string for the primary header label. */
      headerString_: String,

      /* The string for the subtitle. */
      subtitleString_: String,

      /**
       * The text that will be shown in the toast element upon clicking one of
       * the actions.
       */
      toastText_: String,
    };
  }

  private sites_: NotificationPermission[];
  private notificationPermissionReviewListExpanded_: boolean;
  private shouldShowCompletionInfo_: boolean;
  private lastOrigins_: string[] = [];
  private lastUserAction_: Actions|null;
  private headerString_: string;
  private subtitleString_: string;
  private sitesLoaded_: boolean = false;
  private modelUpdateDelayMsForTesting_: number|null = null;
  private toastText_: string|null;
  private eventTracker_: EventTracker = new EventTracker();
  private shouldRefocusExpandButton_: boolean = false;
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    // Register for review notification permission list updates.
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.sites_ = await this.browserProxy_.getNotificationPermissionReview();
    this.sitesLoaded_ = true;

    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));

    // This should be called after the sites have been retrieved such that
    // currentRouteChanged is called afterwards.
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.eventTracker_.removeAll();
  }

  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute !== routes.SITE_SETTINGS_NOTIFICATIONS) {
      return;
    }
    // Only record the metrics when the user navigates to the notification
    // settings page that shows the review notifications module.
    this.metricsBrowserProxy_.recordSafetyCheckNotificationsListCountHistogram(
        this.sites_.length);
    this.metricsBrowserProxy_
        .recordSafetyCheckNotificationsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.OPEN_REVIEW_UI);
  }

  /* Show action menu when clicked to three dot menu. */
  private onShowActionMenuClick_(e: DomRepeatEvent<NotificationPermission>) {
    this.lastOrigins_ = [e.model.item.origin];
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(actionMenu);
    actionMenu.showAt(e.target as HTMLElement);
  }

  private onBlockNotificationPermissionClick_(
      event: DomRepeatEvent<NotificationPermission>) {
    event.stopPropagation();
    const item = event.model.item;
    this.lastOrigins_ = [item.origin];
    this.lastUserAction_ = Actions.BLOCK;
    this.showUndoToast_();
    this.hideItem_(this.lastOrigins_[0]);
    this.metricsBrowserProxy_
        .recordSafetyCheckNotificationsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.BLOCK);
    setTimeout(
        this.browserProxy_.blockNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_),
        this.getModelUpdateDelayMs_());
  }

  private onIgnoreClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.lastUserAction_ = Actions.IGNORE;
    this.showUndoToast_();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.hideItem_(this.lastOrigins_[0]);
    this.metricsBrowserProxy_
        .recordSafetyCheckNotificationsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.IGNORE);
    setTimeout(
        this.browserProxy_.ignoreNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_),
        this.getModelUpdateDelayMs_());
  }

  private onResetClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.lastUserAction_ = Actions.RESET;
    this.showUndoToast_();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.hideItem_(this.lastOrigins_[0]);
    this.metricsBrowserProxy_
        .recordSafetyCheckNotificationsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.RESET);
    setTimeout(
        this.browserProxy_.resetNotificationPermissionForOrigins.bind(
            this.browserProxy_, this.lastOrigins_),
        this.getModelUpdateDelayMs_());
  }

  private onBlockAllClick_(e: Event) {
    e.stopPropagation();
    // To be able to undo the block-all action, we need to keep track of all
    // origins that were blocked.
    this.lastOrigins_ = this.sites_!.map(site => site.origin);
    this.browserProxy_.blockNotificationPermissionForOrigins(this.lastOrigins_);
    this.lastUserAction_ = Actions.BLOCK;
    this.showUndoToast_();
    this.metricsBrowserProxy_
        .recordSafetyCheckNotificationsModuleInteractionsHistogram(
            SafetyCheckNotificationsModuleInteractions.BLOCK_ALL);
  }

  /* Repopulate the list when notification permission list is updated. */
  private onReviewNotificationPermissionListChanged_(
      sites: NotificationPermission[]) {
    this.sites_ = sites;

    // The already rendered <cr-row>s are reused as the model is updated,
    // so we need to reset their CSS classes.
    const rows = this.shadowRoot!.querySelectorAll('.site-list .site-entry');
    for (const row of rows) {
      row.classList.remove('removed');
    }
  }

  private onShowTooltip_(e: Event) {
    e.stopPropagation();
    const tooltip = this.shadowRoot!.querySelector('cr-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target! as Element);
  }

  private async updateNotificationPermissionReviewListExpanded_():
      Promise<void> {
    if (!this.notificationPermissionReviewListExpanded_) {
      // Record metric on user minimising the review list.
      this.metricsBrowserProxy_
          .recordSafetyCheckNotificationsModuleInteractionsHistogram(
              SafetyCheckNotificationsModuleInteractions.MINIMIZE);
    }
  }

  private async updateUndoNotificationText_(): Promise<void> {
    if (!this.lastUserAction_ || this.lastOrigins_.length === 0) {
      return;
    }
    switch (this.lastUserAction_) {
      case Actions.BLOCK:
        if (this.lastOrigins_!.length === 1) {
          this.toastText_ = this.i18n(
              'safetyCheckNotificationPermissionReviewBlockedToastLabel',
              this.lastOrigins_[0]);
        } else {
          this.toastText_ =
              await PluralStringProxyImpl.getInstance().getPluralString(
                  'safetyCheckNotificationPermissionReviewBlockAllToastLabel',
                  this.lastOrigins_.length);
        }
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

  private showUndoToast_(): void {
    // Re-open the toast if one was already open; this resets the timer.
    if (this.$.undoToast.open) {
      this.$.undoToast.hide();
    }
    this.$.undoToast.show();
  }

  private onUndoButtonClick_(e: Event) {
    e.stopPropagation();
    this.undoLastAction_();
  }

  private undoLastAction_() {
    switch (this.lastUserAction_) {
      // As BLOCK and RESET actions just change the notification permission,
      // undoing them only requires allowing notification permissions again.
      case Actions.BLOCK:
        this.browserProxy_.allowNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.lastOrigins_ = [];
        this.metricsBrowserProxy_
            .recordSafetyCheckNotificationsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK);
        break;
      case Actions.RESET:
        this.browserProxy_.allowNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.lastOrigins_ = [];
        this.metricsBrowserProxy_
            .recordSafetyCheckNotificationsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_RESET);
        break;
      case Actions.IGNORE:
        this.browserProxy_.undoIgnoreNotificationPermissionForOrigins(
            this.lastOrigins_);
        this.lastOrigins_ = [];
        this.metricsBrowserProxy_
            .recordSafetyCheckNotificationsModuleInteractionsHistogram(
                SafetyCheckNotificationsModuleInteractions.UNDO_IGNORE);
        break;
      default:
        assertNotReached();
    }

    this.shouldRefocusExpandButton_ = true;
    this.$.undoToast.hide();
  }

  private onKeyDown_(e: KeyboardEvent) {
    // Only allow undoing via ctrl+z when the undo toast is opened.
    if (!this.$.undoToast.open) {
      return;
    }

    if (isUndoKeyboardEvent(e)) {
      this.undoLastAction_();
    }
  }

  private getBlockAriaLabelForOrigin(origin: string): string {
    return this.i18n(
        'safetyCheckNotificationPermissionReviewDontAllowAriaLabel', origin);
  }

  private getIgnoreAriaLabelForOrigins(origins: string[]): string|null {
    // A label is only needed when the action menu is shown for a single origin.
    if (origins.length !== 1) {
      return null;
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewIgnoreAriaLabel', origins[0]);
  }

  private getResetAriaLabelForOrigins(origins: string[]): string|null {
    if (origins.length !== 1) {
      return null;
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewResetAriaLabel', origins[0]);
  }

  private hideItem_(origin?: string) {
    const rows = this.shadowRoot!.querySelectorAll('.site-list .site-entry');

    // Remove the row that corresponds to |origin|. If no origin is specified,
    // remove all rows.
    for (let i = 0; i < this.sites_!.length; ++i) {
      if (!origin || this.sites_![i]!.origin === origin) {
        rows[i]!.classList.add('removed');
        if (origin) {
          break;
        }
      }
    }
  }

  /**
   * Retrieve the list of domains that send lots of notification and implicitly
   * trigger the update of the display list.
   */
  private async onSitesChanged_() {
    assert(this.sites_);
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel',
            this.sites_.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewSecondaryLabel',
            this.sites_.length);
    // Focus on the expand button after the undo button is clicked and sites are
    // loaded again.
    if (this.shouldRefocusExpandButton_) {
      this.shouldRefocusExpandButton_ = false;
      const expandButton = this.shadowRoot!.querySelector('cr-expand-button');
      assert(expandButton);
      expandButton.focus();
    }
  }

  private getMoreActionsAriaLabel_(origin: string): string {
    return this.i18n(
        'safetyCheckNotificationPermissionReviewMoreActionsAriaLabel', origin);
  }

  /** Show info that review is completed when there are no permissions left. */
  private computeShouldShowCompletionInfo_(): boolean {
    return this.sitesLoaded_ && this.sites_.length === 0;
  }

  private getModelUpdateDelayMs_() {
    if (this.modelUpdateDelayMsForTesting_ === null) {
      return MODEL_UPDATE_DELAY_MS;
    } else {
      return this.modelUpdateDelayMsForTesting_;
    }
  }

  setModelUpdateDelayMsForTesting(delayMs: number) {
    this.modelUpdateDelayMsForTesting_ = delayMs;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'review-notification-permissions':
        SettingsReviewNotificationPermissionsElement;
  }
}

customElements.define(
    SettingsReviewNotificationPermissionsElement.is,
    SettingsReviewNotificationPermissionsElement);
