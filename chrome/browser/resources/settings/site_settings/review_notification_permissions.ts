// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './review_notification_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

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
    WebUIListenerMixin(BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement))));

/**
 * Corresponds to the animation-duration CSS parameter defined
 * in review_notification_permissions.html. Set to be slightly higher, as we
 * want to ensure that the animation is finished before updating the model for
 * the right visual effect.
 */
const MODEL_UPDATE_DELAY_MS = 300;

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
      },

      /* The last origin that the user interacted with. */
      lastOrigin_: String,

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
    };
  }

  private sites_: NotificationPermission[];
  private notificationPermissionReviewListExpanded_: boolean;
  private shouldShowCompletionInfo_: boolean;
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private lastOrigin_: string;
  private lastUserAction_: Actions|null;
  private headerString_: string;
  private sitesLoaded_: boolean = false;
  private modelUpdateDelayMsForTesting_: number|null = null;

  override async connectedCallback() {
    super.connectedCallback();
    // Register for review notification permission list updates.
    this.addWebUIListener(
        'notification-permission-review-list-changed',
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.sites_ = await this.browserProxy_.getNotificationPermissionReview();
    this.sitesLoaded_ = true;
  }

  /* Show action menu when clicked to three dot menu. */
  private onShowActionMenuClick_(e: DomRepeatEvent<NotificationPermission>) {
    this.lastOrigin_ = e.model.item.origin;
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(actionMenu);
    actionMenu.showAt(e.target as HTMLElement);
  }

  private onBlockNotificationPermissionClick_(
      event: DomRepeatEvent<NotificationPermission>) {
    event.stopPropagation();
    const item = event.model.item;
    this.lastUserAction_ = Actions.BLOCK;
    this.lastOrigin_ = item.origin;
    this.showUndoToast_();
    this.hideItem_(this.lastOrigin_);
    setTimeout(
        this.browserProxy_.blockNotificationPermissionForOrigin.bind(
            this.browserProxy_, this.lastOrigin_),
        this.getModelUpdateDelayMs_());
  }

  private onIgnoreClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.lastUserAction_ = Actions.IGNORE;
    this.showUndoToast_();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.hideItem_(this.lastOrigin_);
    setTimeout(
        this.browserProxy_.ignoreNotificationPermissionForOrigin.bind(
            this.browserProxy_, this.lastOrigin_),
        this.getModelUpdateDelayMs_());
  }

  private onResetClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.lastUserAction_ = Actions.RESET;
    this.showUndoToast_();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.hideItem_(this.lastOrigin_);
    setTimeout(
        this.browserProxy_.resetNotificationPermissionForOrigin.bind(
            this.browserProxy_, this.lastOrigin_),
        this.getModelUpdateDelayMs_());
  }

  /* Repopulate the list when notification permission list is updated. */
  private onReviewNotificationPermissionListChanged_(
      sites: NotificationPermission[]) {
    this.sites_ = sites;

    // The already rendered <cr-row>s are reused as the model is updated,
    // so we need to reset their CSS classes.
    const rows = this.shadowRoot!.querySelectorAll(
        '.notification-permissions-list .cr-row');
    for (const row of rows) {
      row.classList.remove('removed');
    }
  }

  private onShowTooltip_(e: Event) {
    e.stopPropagation();
    const target = e.target!;
    const tooltip = this.shadowRoot!.querySelector('paper-tooltip');
    assert(tooltip);
    tooltip.target = target;
    tooltip.updatePosition();
    const hide = () => {
      tooltip.hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('click', hide);
      tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('click', hide);
    tooltip.addEventListener('mouseenter', hide);
    tooltip.show();
  }

  private getUndoNotificationText_(): string {
    if (!this.lastUserAction_ || !this.lastOrigin_) {
      return '';
    }
    switch (this.lastUserAction_) {
      case Actions.BLOCK:
        return this.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            this.lastOrigin_);
      case Actions.IGNORE:
        return this.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            this.lastOrigin_);
      case Actions.RESET:
        return this.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            this.lastOrigin_);
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
    switch (this.lastUserAction_) {
      // As BLOCK and RESET actions just change the notification permission,
      // undoing them only requires allowing notification permissions again.
      case Actions.BLOCK:
      case Actions.RESET:
        this.browserProxy_.allowNotificationPermissionForOrigin(
            this.lastOrigin_);
        break;
      case Actions.IGNORE:
        this.browserProxy_.undoIgnoreNotificationPermissionForOrigin(
            this.lastOrigin_);
        break;
    }
    this.$.undoToast.hide();
  }

  private getBlockAriaLabelForOrigin(origin: string): string {
    return this.i18n(
        'safetyCheckNotificationPermissionReviewDontAllowAriaLabel', origin);
  }

  private getIgnoreAriaLabelForOrigin(lastOrigin: string): string|null {
    // At the time of initialization, lastOrigin is null and we do not need an
    // aria label yet.
    if (!lastOrigin) {
      return null;
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewIgnoreAriaLabel', lastOrigin);
  }

  private getResetAriaLabelForOrigin(lastOrigin: string): string|null {
    if (!lastOrigin) {
      return null;
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewResetAriaLabel', lastOrigin);
  }

  private hideItem_(origin?: string) {
    const rows = this.shadowRoot!.querySelectorAll(
        '.notification-permissions-list .cr-row');

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
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel',
            this.sites_!.length);
  }

  private getMoreActionsAriaLabel_(lastOrigin: string): string|null {
    if (!lastOrigin) {
      return null;
    }
    return this.i18n(
        'safetyCheckNotificationPermissionReviewMoreActionsAriaLabel',
        lastOrigin);
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
