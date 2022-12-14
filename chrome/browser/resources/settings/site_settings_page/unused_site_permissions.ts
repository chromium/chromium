// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import '../i18n_setup.js';
import './site_review_shared.css.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSettingsTypes, MODEL_UPDATE_DELAY_MS} from '../site_settings/constants.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';
import {SiteSettingsPermissionsBrowserProxy, SiteSettingsPermissionsBrowserProxyImpl, UnusedSitePermissions} from '../site_settings/site_settings_permissions_browser_proxy.js';
import {TooltipMixin} from '../tooltip_mixin.js';

import {getLocalizationStringForContentType} from './site_settings_page_util.js';
import {getTemplate} from './unused_site_permissions.html.js';

export interface SettingsUnusedSitePermissionsElement {
  $: {
    undoToast: CrToastElement,
  };
}

/**
 * Information about unused site permissions with an additional flag controlling
 * the removal animation.
 */
interface UnusedSitePermissionsDisplay extends UnusedSitePermissions {
  visible: boolean;
}

const SettingsUnusedSitePermissionsElementBase = TooltipMixin(
    I18nMixin(WebUiListenerMixin(SiteSettingsMixin(PolymerElement))));

export class SettingsUnusedSitePermissionsElement extends
    SettingsUnusedSitePermissionsElementBase {
  static get is() {
    return 'settings-unused-site-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The string for the primary header label. */
      headerString_: String,

      /**
       * List of unused sites where permissions have been removed. This list
       * being null indicates it has not loaded yet.
       */
      sites_: {
        type: Array,
        value: null,
        observer: 'onSitesChanged_',
      },

      /**
       * Indicates whether to show completion info after user has finished the
       * review process.
       */
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed: 'computeShouldShowCompletionInfo_(sites_.*)',
      },

      /** Text below primary header label. */
      subtitleString_: String,

      /* The text that will be shown in the undo toast element. */
      toastText_: String,

      /* If the list of unused site permissions is expanded or collapsed. */
      unusedSitePermissionsReviewListExpanded_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private browserProxy_: SiteSettingsPermissionsBrowserProxy =
      SiteSettingsPermissionsBrowserProxyImpl.getInstance();
  private headerString_: string;
  private modelUpdateDelayMsForTesting_: number|null = null;
  private sites_: UnusedSitePermissionsDisplay[]|null;
  private shouldShowCompletionInfo_: boolean;
  private subtitleString_: string;
  private toastText_: string|null;
  private unusedSitePermissionsReviewListExpanded_: boolean;

  override async connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'unused-permission-review-list-maybe-changed',
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    const sites =
        await this.browserProxy_.getRevokedUnusedSitePermissionsList();
    this.onUnusedSitePermissionListChanged_(sites);
  }

  /** Show info that review is completed when there are no permissions left. */
  private computeShouldShowCompletionInfo_(): boolean {
    return this.sites_ !== null && this.sites_.length === 0;
  }

  private getAllowAgainAriaLabelForOrigin_(origin: string): string {
    return this.i18n(
        'safetyCheckUnusedSitePermissionsAllowAgainAriaLabel', origin);
  }

  // TODO(crbug.com/1393005): Refactor common code across this and
  // review_notification_permissions.ts.
  private getModelUpdateDelayMs_() {
    return this.modelUpdateDelayMsForTesting_ === null ?
        MODEL_UPDATE_DELAY_MS :
        this.modelUpdateDelayMsForTesting_;
  }

  /**
   * Text that describes which permissions have been revoked for an origin.
   * Permissions are listed explicitly when there are up to and including 3. For
   * 4 or more, the two first permissions are listed explicitly and for the
   * remaining ones a count is shown, e.g. 'and 2 more'.
   */
  private getPermissionsText_(permissions: ContentSettingsTypes[]): string {
    assert(
        permissions.length > 0,
        'There is no permission for the user to review.');

    const permissionsI18n = permissions.map(permission => {
      const localizationString =
          getLocalizationStringForContentType(permission);
      assert(localizationString !== null);
      return this.i18n(localizationString);
    });

    if (permissionsI18n.length === 1) {
      return this.i18n(
          'safetyCheckUnusedSitePermissionsRemovedOnePermissionLabel',
          ...permissionsI18n);
    }
    if (permissionsI18n.length === 2) {
      return this.i18n(
          'safetyCheckUnusedSitePermissionsRemovedTwoPermissionsLabel',
          ...permissionsI18n);
    }
    if (permissionsI18n.length === 3) {
      return this.i18n(
          'safetyCheckUnusedSitePermissionsRemovedThreePermissionsLabel',
          ...permissionsI18n);
    }
    return this.i18n(
        'safetyCheckUnusedSitePermissionsRemovedFourOrMorePermissionsLabel',
        permissionsI18n[0], permissionsI18n[1], permissionsI18n.length - 2);
  }

  private getRowClass_(visible: boolean): string {
    return visible ? '' : 'removed';
  }

  // TODO(crbug.com/1393005): Refactor common code across this and
  // review_notification_permissions.ts.
  private hideItem_(origin?: string) {
    assert(this.sites_ !== null);
    for (const [index, site] of this.sites_.entries()) {
      if (!origin || site.origin === origin) {
        // Update site property through Polymer's array mutation method so
        // that the corresponding row in the dom-repeat for the list of sites
        // gets notified.
        this.set(['sites_', index, 'visible'], false);
        if (origin) {
          break;
        }
      }
    }
  }

  private onAllowAgainClick_(event: DomRepeatEvent<UnusedSitePermissions>) {
    event.stopPropagation();
    const item = event.model.item;
    this.showUndoToast_(
        this.i18n('safetyCheckUnusedSitePermissionsToastLabel', item.origin));
    this.hideItem_(item.origin);
    setTimeout(
        this.browserProxy_.allowPermissionsAgainForUnusedSite.bind(
            this.browserProxy_, item),
        this.getModelUpdateDelayMs_());
  }

  private async onGotItClick_(e: Event) {
    e.stopPropagation();
    assert(this.sites_ !== null);

    this.browserProxy_.acknowledgeRevokedUnusedSitePermissionsList(this.sites_);
    const toastText = await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', this.sites_.length);
    this.showUndoToast_(toastText);
  }

  /* Repopulate the list when unused site permission list is updated. */
  private onUnusedSitePermissionListChanged_(sites: UnusedSitePermissions[]) {
    this.sites_ = sites.map(
        (site: UnusedSitePermissions): UnusedSitePermissionsDisplay => {
          return {...site, visible: true};
        });
  }

  private onShowTooltip_(e: Event) {
    e.stopPropagation();
    const tooltip = this.shadowRoot!.querySelector('paper-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target!);
  }

  private async onSitesChanged_() {
    if (this.sites_ === null) {
      return;
    }

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsPrimaryLabel', this.sites_.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsSecondaryLabel',
            this.sites_.length);
  }

  private showUndoToast_(text: string) {
    this.toastText_ = text;
    // Re-open the toast if one was already open; this resets the timer.
    if (this.$.undoToast.open) {
      this.$.undoToast.hide();
    }
    this.$.undoToast.show();
  }

  // TODO(crbug.com/1393005): Refactor common code across this and
  // review_notification_permissions.ts.
  setModelUpdateDelayMsForTesting(delayMs: number) {
    this.modelUpdateDelayMsForTesting_ = delayMs;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-unused-site-permissions': SettingsUnusedSitePermissionsElement;
  }
}

customElements.define(
    SettingsUnusedSitePermissionsElement.is,
    SettingsUnusedSitePermissionsElement);
