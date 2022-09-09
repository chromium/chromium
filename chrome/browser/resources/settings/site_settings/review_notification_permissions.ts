// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './review_notification_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

export interface SettingsReviewNotificationPermissionsElement {
  $: {
    tooltip: PaperTooltipElement,
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SettingsReviewNotificationPermissionsElementBase =
    BaseMixin(SiteSettingsMixin(PolymerElement));

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
      reviewNotificationPermissionsList_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private reviewNotificationPermissionsList_: NotificationPermission[];

  /**
   * Load the review notification permission list whenever the page is
   * loaded.
   */
  override connectedCallback() {
    super.connectedCallback();
    this.populateList_();
  }

  /**
   * @return a user-friendly name for the origin that is granted with
   *     notification permission.
   */
  private getDisplayName_(notificationPermission: NotificationPermission):
      string {
    return this.toUrl(notificationPermission.origin)!.host;
  }

  /**
   * @return the site scheme for the origin that is granted with notification
   *     permission.
   */
  private getSiteScheme_({origin}: NotificationPermission): string {
    const scheme = this.toUrl(origin)!.protocol.slice(0, -1);
    return scheme === 'https' ? '' : scheme;
  }

  /**
   * @return the correct CSS class to apply depending on this notification
   *     permissions entry based on the index.
   */
  private getClassForIndex_(index: number): string {
    return index === 0 ? 'first' : '';
  }

  /* Show action menu when clicked to three dot menu. */
  private onShowActionMenuClick_(e: Event) {
    this.$.actionMenu.get().showAt(e.target as HTMLElement);
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

  /**
   * Retrieve the list of domains that sned lots of notification and implicitly
   * trigger the update of the display list.
   */
  private async populateList_() {
    const browserProxy = SiteSettingsPrefsBrowserProxyImpl.getInstance();
    this.reviewNotificationPermissionsList_ =
        await browserProxy.getReviewNotificationPermissions();
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
