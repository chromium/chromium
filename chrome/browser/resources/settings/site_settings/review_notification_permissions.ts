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

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './review_notification_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

export interface SettingsReviewNotificationPermissionsElement {
  $: {
    tooltip: PaperTooltipElement,
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    undoToast: CrToastElement,
  };
}

/**
 * The list of actions that a user can take with regards to the permissions of
 * notifications.
 */
enum ACTIONS {
  BLOCK = 'block',
  IGNORE = 'ignore',
  RESET = 'reset'
}

const SettingsReviewNotificationPermissionsElementBase =
    WebUIListenerMixin(BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement))));

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
      notificationPermissionReviewList_: {
        type: Array,
        value: () => [],
      },

      /* The site for the currently open action menu. */
      actionMenuSite_: Object,
    };
  }

  private notificationPermissionReviewList_: NotificationPermission[];
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private actionMenuSite_: NotificationPermission|null;
  private lastUserAction_: ACTIONS|null;

  override connectedCallback() {
    super.connectedCallback();
    // Register for review notification permission list updates.
    this.addWebUIListener(
        'notification-permission-review-list-changed',
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.populateList_();
  }

  /**
   * @return a user-friendly name for the primary pattern that is granted with
   *     notification permission.
   */
  private getDisplayName_(notificationPermission: NotificationPermission):
      string {
    return this.toUrl(notificationPermission.origin)!.host;
  }

  /**
   * @return the correct CSS class to apply depending on this notification
   *     permissions entry based on the index.
   */
  private getClassForIndex_(index: number): string {
    return index === 0 ? 'first' : '';
  }

  /* Show action menu when clicked to three dot menu. */
  private onShowActionMenuClick_(e: DomRepeatEvent<NotificationPermission>) {
    this.actionMenuSite_ = e.model.item;
    this.$.actionMenu.get().showAt(e.target as HTMLElement);
  }

  private onBlockNotificationPermissionClick_(
      event: DomRepeatEvent<NotificationPermission>) {
    event.stopPropagation();
    const item = event.model.item;
    this.browserProxy_.blockNotificationPermissionForOrigin(item.origin);
    this.lastUserAction_ = ACTIONS.BLOCK;
    this.actionMenuSite_ = item;
    this.showUndoToast_();
  }

  private onIgnoreClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.browserProxy_.ignoreNotificationPermissionForOrigin(
        this.actionMenuSite_!.origin);
    this.lastUserAction_ = ACTIONS.IGNORE;
    this.showUndoToast_();
    this.$.actionMenu.get().close();
  }

  private onResetClick_(e: DomRepeatEvent<NotificationPermission>) {
    e.stopPropagation();
    this.browserProxy_.resetNotificationPermissionForOrigin(
        this.actionMenuSite_!.origin);
    this.lastUserAction_ = ACTIONS.RESET;
    this.showUndoToast_();
    this.$.actionMenu.get().close();
  }

  /* Repopulate the list when notification permission list is updated. */
  private onReviewNotificationPermissionListChanged_(
      sites: NotificationPermission[]) {
    this.notificationPermissionReviewList_ = sites;
  }

  private onShowTooltip_(e: Event) {
    e.stopPropagation();
    const target = e.target!;
    const tooltip = this.$.tooltip;
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
    if (!this.lastUserAction_ || !this.actionMenuSite_) {
      return '';
    }
    switch (this.lastUserAction_) {
      case ACTIONS.BLOCK: {
        return this.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            this.actionMenuSite_!.origin);
      }
      case ACTIONS.IGNORE: {
        return this.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            this.actionMenuSite_!.origin);
      }
      case ACTIONS.RESET: {
        return this.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            this.actionMenuSite_!.origin);
      }
      default: {
        assertNotReached();
      }
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
    // TODO(crbug/1345920) add functionality to undo user action
  }

  /**
   * Retrieve the list of domains that send lots of notification and implicitly
   * trigger the update of the display list.
   */
  private async populateList_() {
    this.notificationPermissionReviewList_ =
        await this.browserProxy_.getNotificationPermissionReview();
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
