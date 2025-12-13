// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../i18n_setup.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import './safety_hub_module.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {isUndoKeyboardEvent} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckUnusedSitePermissionsModuleInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {ContentSettingsTypes} from '../site_settings/constants.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';
import {getLocalizationStringForContentType} from '../site_settings/site_settings_util.js';
import {TooltipMixin} from '../tooltip_mixin.js';

import type {SafetyHubBrowserProxy, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {PermissionsRevocationType, SafetyHubBrowserProxyImpl, SafetyHubEvent} from './safety_hub_browser_proxy.js';
import type {SettingsSafetyHubModuleElement, SiteInfo} from './safety_hub_module.js';
import {getTemplate} from './unused_site_permissions_module.html.js';

export interface SettingsSafetyHubUnusedSitePermissionsModuleElement {
  $: {
    headerActionMenu: CrActionMenuElement,
    bulkUndoButton: HTMLElement,
    gotItButton: HTMLElement,
    goToSettings: HTMLElement,
    module: SettingsSafetyHubModuleElement,
    moreActionButton: HTMLElement,
    toastUndoButton: HTMLElement,
    undoToast: CrToastElement,
  };
}

/** Actions the user can perform to review their unused site permissions. */
enum Action {
  ALLOW_AGAIN,
  GOT_IT,
}

/**
 * Information about unused site permissions with an additional detail field to
 * extend SiteInfo.
 */
interface UnusedSitePermissionsDisplay extends UnusedSitePermissions, SiteInfo {
  detail: string;
}

const SettingsSafetyHubUnusedSitePermissionsModuleElementBase =
    TooltipMixin(I18nMixin(RouteObserverMixin(
        WebUiListenerMixin(SiteSettingsMixin(PolymerElement)))));

function doNothing() {}

export class SettingsSafetyHubUnusedSitePermissionsModuleElement extends
    SettingsSafetyHubUnusedSitePermissionsModuleElementBase {
  static get is() {
    return 'settings-safety-hub-unused-site-permissions-module';
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

      // Most recent site permissions the user has allowed again.
      lastUnusedSitePermissionsAllowedAgain_: {
        type: Object,
        value: null,
      },

      // Most recent site permissions list the user has acknowledged.
      lastUnusedSitePermissionsListAcknowledged_: {
        type: Array,
        value: null,
      },

      // Sites that have already been rendered. Any new ones not listed here
      // will need to be explicitly animated to show.
      renderedOrigins_: {
        type: Array,
        value: [],
      },

      // Last action the user has taken, determines the function of the undo
      // button in the toast.
      lastUserAction_: {
        type: Object,
        value: null,
      },

      // List of unused sites where permissions have been removed. This list
      // being null indicates it has not loaded yet.
      sites_: {
        type: Array,
        value: null,
        observer: 'onSitesChanged_',
      },

      // The text that will be shown in the undo toast element.
      toastText_: String,

      // Indicates whether user has finished the review process.
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed: 'computeShouldShowCompletionInfo_(sites_.*)',
      },
    };
  }

  declare private headerString_: string;
  declare private subheaderString_: string|null;
  declare private toastText_: string|null;
  declare private sites_: UnusedSitePermissionsDisplay[]|null;
  declare private shouldShowCompletionInfo_: boolean;
  declare private lastUnusedSitePermissionsAllowedAgain_: UnusedSitePermissions|
      null;
  declare private lastUnusedSitePermissionsListAcknowledged_:
      UnusedSitePermissions[]|null;
  declare private renderedOrigins_: string[];
  declare private lastUserAction_: Action|null;
  private pendingFocusCallback_: Function = doNothing;
  private eventTracker_: EventTracker = new EventTracker();
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();


  override async connectedCallback() {
    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    const sites =
        await this.browserProxy_.getRevokedUnusedSitePermissionsList();

    this.onUnusedSitePermissionListChanged_(sites);
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
          .recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(
              this.sites_.length);
    }

    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
  }

  /**
   * Text that describes which permissions have been revoked for an origin.
   * Permissions are listed explicitly when there are up to and including 3.
   * For 4 or more, the two first permissions are listed explicitly and for
   * the remaining ones a count is shown, e.g. 'and 2 more'.
   */
  private getPermissionsText_(
      revocationType: PermissionsRevocationType,
      permissions: ContentSettingsTypes[]): string {
    assert(
        permissions.length > 0,
        'There is no permission for the user to review.');

    const permissionsI18n = permissions.map(permission => {
      const localizationString =
          getLocalizationStringForContentType(permission);
      return localizationString ? this.i18n(localizationString) : '';
    });

    // For abusive revocations, use a string that mentions abusive sites.
    // Disruptive and unused revocations share the same string that mentions not
    // visiting the site.
    if (revocationType ===
            PermissionsRevocationType.ABUSIVE_NOTIFICATION_PERMISSIONS ||
        revocationType ===
            PermissionsRevocationType
                .UNUSED_PERMISSIONS_AND_ABUSIVE_NOTIFICATIONS) {
      return this.i18n(
          'safetyHubAbusiveNotificationPermissionsSettingSublabel');
    }

    switch (permissionsI18n.length) {
      case 1:
        return this.i18n(
            'safetyHubUnusedSitePermissionsRemovedOnePermissionLabel',
            ...permissionsI18n);
      case 2:
        return this.i18n(
            'safetyHubUnusedSitePermissionsRemovedTwoPermissionsLabel',
            ...permissionsI18n);
      case 3:
        return this.i18n(
            'safetyHubUnusedSitePermissionsRemovedThreePermissionsLabel',
            ...permissionsI18n);
      default:
        return this.i18n(
            'safetyHubUnusedSitePermissionsRemovedFourOrMorePermissionsLabel',
            permissionsI18n[0], permissionsI18n[1], permissionsI18n.length - 2);
    }
  }

  /** Clears all the changes made by a previous action. */
  private resetValues(event: Event) {
    event.stopPropagation();
    this.$.undoToast.hide();
  }

  private onAllowAgainClick_(event: CustomEvent<UnusedSitePermissions>) {
    this.resetValues(event);

    // Set values needed for the action.
    const item = event.detail;
    this.lastUserAction_ = Action.ALLOW_AGAIN;
    this.lastUnusedSitePermissionsAllowedAgain_ = item;

    // Update the toastText_ that isused both as an undo toast text and as a
    // header text.
    this.toastText_ =
        this.i18n('safetyHubUnusedSitePermissionsToastLabel', item.origin);
    // Only show Undo toast if there are multiple sites to review. Otherwise,
    // once the single site is reviewed, the completion state with a permanent
    // Undo button in the header will be shown.
    if (this.sites_!.length > 1) {
      this.$.undoToast.show().then(() => {
        this.$.toastUndoButton.focus();
      });
    }
    this.$.module.animateHide(
        item.origin,
        this.browserProxy_.allowPermissionsAgainForUnusedSite.bind(
            this.browserProxy_, item.origin));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
            SafetyCheckUnusedSitePermissionsModuleInteractions.ALLOW_AGAIN);

    if (this.doesSiteListIncludeAbusiveNotifications([item])) {
      this.metricsBrowserProxy_
          .recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
              SafetyCheckUnusedSitePermissionsModuleInteractions.ALLOW_AGAIN);
    }
  }

  private async onGotItClick_(e: Event) {
    this.resetValues(e);

    // Set values needed for the action.
    assert(this.sites_ !== null);
    this.lastUserAction_ = Action.GOT_IT;
    this.lastUnusedSitePermissionsListAcknowledged_ = this.sites_;

    // Update the toastText_ that is also used as a header text.
    this.toastText_ = await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyHubUnusedSitePermissionsToastBulkLabel', this.sites_.length);

    this.$.module.animateHide(
        /* all origins */ null,
        this.browserProxy_.acknowledgeRevokedUnusedSitePermissionsList.bind(
            this.browserProxy_));

    this.browserProxy_.recordSafetyHubInteraction();
    this.metricsBrowserProxy_
        .recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
            SafetyCheckUnusedSitePermissionsModuleInteractions.ACKNOWLEDGE_ALL);

    if (this.doesSiteListIncludeAbusiveNotifications(this.sites_)) {
      this.metricsBrowserProxy_
          .recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
              SafetyCheckUnusedSitePermissionsModuleInteractions
                  .ACKNOWLEDGE_ALL);
    }
  }

  private onMoreActionClick_(e: Event) {
    e.stopPropagation();
    this.$.headerActionMenu.showAt(e.target as HTMLElement);
  }

  private onGoToSettingsClick_(e: Event) {
    e.stopPropagation();
    this.$.headerActionMenu.close();
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);

    this.metricsBrowserProxy_
        .recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
            SafetyCheckUnusedSitePermissionsModuleInteractions.GO_TO_SETTINGS);

    if (this.sites_ && this.sites_.length > 0 &&
        this.doesSiteListIncludeAbusiveNotifications(this.sites_)) {
      this.metricsBrowserProxy_
          .recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
              SafetyCheckUnusedSitePermissionsModuleInteractions
                  .GO_TO_SETTINGS);
    }
  }

  /* Repopulate the list when unused site permission list is updated. */
  private onUnusedSitePermissionListChanged_(sites: UnusedSitePermissions[]) {
    this.sites_ = sites.map(
        (site: UnusedSitePermissions): UnusedSitePermissionsDisplay => ({
          ...site,
          detail:
              this.getPermissionsText_(site.revocationType, site.permissions),
        }));
  }

  private setHeaderToCompletionState_() {
    assert(this.headerString_);
    this.headerString_ = this.toastText_!;
    this.subheaderString_ = '';
    this.$.bulkUndoButton.focus();
  }

  private async onSitesChanged_() {
    if (this.sites_ === null) {
      return;
    }

    const callback = this.pendingFocusCallback_;
    this.pendingFocusCallback_ = doNothing;

    this.$.module.animateShow(
        // Run the show animation on all new items, i.e. those items
        // in |this.sites_| which aren't already rendered.
        this.sites_.map(site => site.origin)
            .filter(origin => !this.renderedOrigins_.includes(origin)),
        callback);
    this.renderedOrigins_ = this.sites_.map(site => site.origin);

    if (this.shouldShowCompletionInfo_) {
      this.setHeaderToCompletionState_();
      return;
    }

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubUnusedSitePermissionsPrimaryLabel', this.sites_.length);
    this.subheaderString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubRevokedPermissionsSecondaryLabel', this.sites_.length);
  }

  private onUndoClick_(e: Event) {
    e.stopPropagation();
    this.undoLastAction_();
  }

  /**
   * Show info that review is completed when there are no permissions left.
   */
  private computeShouldShowCompletionInfo_(): boolean {
    return this.sites_ !== null && this.sites_.length === 0;
  }

  private undoLastAction_() {
    switch (this.lastUserAction_) {
      case Action.ALLOW_AGAIN:
        assert(this.lastUnusedSitePermissionsAllowedAgain_ !== null);
        this.browserProxy_.undoAllowPermissionsAgainForUnusedSite(
            this.lastUnusedSitePermissionsAllowedAgain_);
        if (this.doesSiteListIncludeAbusiveNotifications(
                [this.lastUnusedSitePermissionsAllowedAgain_])) {
          this.metricsBrowserProxy_
              .recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
                  SafetyCheckUnusedSitePermissionsModuleInteractions
                      .UNDO_ALLOW_AGAIN);
        }
        const originToFocus =
            this.lastUnusedSitePermissionsAllowedAgain_.origin;
        this.pendingFocusCallback_ = () => {
          this.$.module.focusOriginMainButton(originToFocus);
        };
        this.lastUnusedSitePermissionsAllowedAgain_ = null;
        this.metricsBrowserProxy_
            .recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
                SafetyCheckUnusedSitePermissionsModuleInteractions
                    .UNDO_ALLOW_AGAIN);
        break;
      case Action.GOT_IT:
        assert(this.lastUnusedSitePermissionsListAcknowledged_ !== null);
        this.browserProxy_.undoAcknowledgeRevokedUnusedSitePermissionsList(
            this.lastUnusedSitePermissionsListAcknowledged_);
        if (this.doesSiteListIncludeAbusiveNotifications(
                this.lastUnusedSitePermissionsListAcknowledged_)) {
          this.metricsBrowserProxy_
              .recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
                  SafetyCheckUnusedSitePermissionsModuleInteractions
                      .UNDO_ACKNOWLEDGE_ALL);
        }
        this.lastUnusedSitePermissionsListAcknowledged_ = null;
        this.metricsBrowserProxy_
            .recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
                SafetyCheckUnusedSitePermissionsModuleInteractions
                    .UNDO_ACKNOWLEDGE_ALL);
        this.pendingFocusCallback_ = () => {
          this.$.gotItButton.focus();
        };
        break;
      default:
        assertNotReached();
    }
    this.lastUserAction_ = null;
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

  // TODO(crbug.com/40267370): Move common functionality between
  // unused_site_permissions_module.ts and notification_permissions_module.ts to
  // a util class.
  private showUndoTooltip_(e: Event) {
    e.stopPropagation();
    const tooltip = this.shadowRoot!.querySelector('cr-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target! as Element);
  }

  private doesSiteListIncludeAbusiveNotifications(
      sites: UnusedSitePermissions[]) {
    // Return true if any of the permission includes abusive type.
    return sites.map(site => site.revocationType)
               .filter(
                   type => type ===
                           PermissionsRevocationType
                               .ABUSIVE_NOTIFICATION_PERMISSIONS ||
                       type ===
                           PermissionsRevocationType
                               .UNUSED_PERMISSIONS_AND_ABUSIVE_NOTIFICATIONS)
               .length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-unused-site-permissions-module':
        SettingsSafetyHubUnusedSitePermissionsModuleElement;
  }
}

customElements.define(
    SettingsSafetyHubUnusedSitePermissionsModuleElement.is,
    SettingsSafetyHubUnusedSitePermissionsModuleElement);
