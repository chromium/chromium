// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 */
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import './all_sites_icons.html.js';
import './clear_storage_dialog_shared.css.js';
import './site_details_permission.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './site_details.html.js';
import {SiteDetailsPermissionElement} from './site_details_permission.js';
import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {WebsiteUsageBrowserProxy, WebsiteUsageBrowserProxyImpl} from './website_usage_browser_proxy.js';

export interface SiteDetailsElement {
  $: {
    confirmClearStorage: CrDialogElement,
    confirmResetSettings: CrDialogElement,
    fpsMembership: HTMLElement,
    noStorage: HTMLElement,
    storage: HTMLElement,
    usage: HTMLElement,
  };
}

const SiteDetailsElementBase =
    RouteObserverMixin(
        SiteSettingsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUiListenerMixinInterface & SiteSettingsMixinInterface &
          RouteObserverMixinInterface,
    };

export class SiteDetailsElement extends SiteDetailsElementBase {
  static get is() {
    return 'site-details';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether unified autoplay blocking is enabled.
       */
      blockAutoplayEnabled: Boolean,

      /**
       * Use the string representing the origin or extension name as the page
       * title of the settings-subpage parent.
       */
      pageTitle: {
        type: String,
        notify: true,
      },

      /**
       * The origin that this widget is showing details for.
       */
      origin_: String,

      /**
       * The amount of data stored for the origin.
       */
      storedData_: {
        type: String,
        value: '',
      },

      /**
       * The number of cookies stored for the origin.
       */
      numCookies_: {
        type: String,
        value: '',
      },

      /**
       * The first party set info for a site including owner and members count.
       */
      fpsMembership_: {
        type: String,
        value: '',
      },

      /**
       * Mock preference used to power managed policy icon for first party sets.
       */
      fpsEnterprisePref_: Object,

      enableExperimentalWebPlatformFeatures_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableExperimentalWebPlatformFeatures');
        },
      },

      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },

      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },
    };
  }

  blockAutoplayEnabled: boolean;
  pageTitle: string;
  private origin_: string;
  private storedData_: string;
  private numCookies_: string;
  private fpsMembership_: string;
  private fpsEnterprisePref_: chrome.settingsPrivate.PrefObject;
  private enableExperimentalWebPlatformFeatures_: boolean;
  private enableWebBluetoothNewPermissionsBackend_: boolean;

  private fetchingForHost_: string = '';
  private websiteUsageProxy_: WebsiteUsageBrowserProxy =
      WebsiteUsageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'usage-total-changed',
        (host: string, data: string, cookies: string, fps: string,
         fpsPolicy: boolean) => {
          this.onUsageTotalChanged_(host, data, cookies, fps, fpsPolicy);
        });

    this.addWebUiListener(
        'contentSettingSitePermissionChanged',
        (category: ContentSettingsTypes, origin: string) =>
            this.onPermissionChanged_(category, origin));

    // Refresh block autoplay status from the backend.
    this.browserProxy.fetchBlockAutoplayStatus();
  }

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route) {
    if (route !== routes.SITE_SETTINGS_SITE_DETAILS) {
      return;
    }
    const site = Router.getInstance().getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.origin_ = site;
    this.browserProxy.isOriginValid(this.origin_).then((valid) => {
      if (!valid) {
        Router.getInstance().navigateToPreviousRoute();
      } else {
        this.fetchingForHost_ = this.toUrl(this.origin_)!.hostname;
        this.storedData_ = '';
        this.websiteUsageProxy_.fetchUsageTotal(this.fetchingForHost_);
        this.browserProxy.getCategoryList(this.origin_).then((categoryList) => {
          this.updatePermissions_(categoryList, /*hideOthers=*/ true);
        });
      }
    });
  }

  /**
   * Called when a site within a category has been changed.
   * @param category The category that changed.
   * @param origin The origin of the site that changed.
   */
  private onPermissionChanged_(category: ContentSettingsTypes, origin: string) {
    if (this.origin_ === undefined || this.origin_ === '' ||
        origin === undefined || origin === '') {
      return;
    }

    this.browserProxy.getCategoryList(this.origin_).then((categoryList) => {
      if (categoryList.includes(category)) {
        this.updatePermissions_([category], /*hideOthers=*/ false);
      }
    });
  }

  /**
   * Callback for when the usage total is known.
   * @param host The host that the usage was fetched for.
   * @param usage The string showing how much data the given host is using.
   * @param cookies The string showing how many cookies the given host is using.
   * @param fpsMembership The string showing first party set membership details.
   * @param fpsPolicy Whether a policy is applied to this FPS member.
   */
  private onUsageTotalChanged_(
      host: string, usage: string, cookies: string, fpsMembership: string,
      fpsPolicy: boolean) {
    if (this.fetchingForHost_ === host) {
      this.storedData_ = usage;
      this.numCookies_ = cookies;
      this.fpsMembership_ = fpsMembership;
      this.fpsEnterprisePref_ = fpsPolicy ? Object.assign({
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      }) :
                                            undefined;
    }
  }

  /**
   * Retrieves the permissions listed in |categoryList| from the backend for
   * |this.origin_|.
   * @param categoryList The list of categories to update permissions for.
   * @param hideOthers If true, permissions for categories not in
   *     |categoryList| will be hidden.
   */
  private updatePermissions_(
      categoryList: ContentSettingsTypes[], hideOthers: boolean) {
    const permissionsMap: {[key: string]: SiteDetailsPermissionElement} =
        Array.prototype.reduce.call(
            this.shadowRoot!.querySelectorAll('site-details-permission'),
            (map, element) => {
              if (categoryList.includes(element.category)) {
                (map as {
                  [key: string]: SiteDetailsPermissionElement,
                })[element.category] = element;
              } else if (hideOthers) {
                // This will hide any permission not in the category list.
                element.site = null;
              }
              return map;
            },
            {}) as {[key: string]: SiteDetailsPermissionElement};

    this.browserProxy.getOriginPermissions(this.origin_, categoryList)
        .then((exceptionList) => {
          exceptionList.forEach((exception, i) => {
            // |exceptionList| should be in the same order as
            // |categoryList|.
            if (permissionsMap[categoryList[i]]) {
              permissionsMap[categoryList[i]].site = exception;
            }
          });

          // The displayName won't change, so just use the first
          // exception.
          assert(exceptionList.length > 0);
          this.pageTitle = exceptionList[0].isolatedWebAppName ??
              this.originRepresentation(exceptionList[0].displayName);
        });
  }

  private onCloseDialog_(e: Event) {
    (e.target as HTMLElement).closest('cr-dialog')!.close();
  }

  /**
   * Confirms the resetting of all content settings for an origin.
   */
  private onConfirmClearSettings_(e: Event) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  }

  /**
   * Confirms the clearing of storage for an origin.
   */
  private onConfirmClearStorage_(e: Event) {
    e.preventDefault();
    this.$.confirmClearStorage.showModal();
  }

  /**
   * Resets all permissions for the current origin.
   */
  private onResetSettings_(e: Event) {
    this.browserProxy.setOriginPermissions(
        this.origin_, null, ContentSetting.DEFAULT);

    this.onCloseDialog_(e);
  }

  /**
   * Clears all data stored, except cookies, for the current origin.
   */
  private onClearStorage_(e: Event) {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.SITE_DETAILS_CLEAR_DATA);
    if (this.hasUsage_(this.storedData_, this.numCookies_)) {
      this.websiteUsageProxy_.clearUsage(this.toUrl(this.origin_)!.href);
      this.storedData_ = '';
      this.numCookies_ = '';
    }

    this.onCloseDialog_(e);
  }

  /**
   * Checks whether this site has any usage information to show.
   * @return Whether there is any usage information to show (e.g. disk or
   *     battery).
   */
  private hasUsage_(storage: string, cookies: string): boolean {
    return storage !== '' || cookies !== '';
  }

  /**
   * Checks whether this site has both storage and cookies information to show.
   * @return Whether there are both storage and cookies information to show.
   */
  private hasDataAndCookies_(storage: string, cookies: string): boolean {
    return storage !== '' && cookies !== '';
  }

  private onResetSettingsDialogClosed_() {
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#resetSettingsButton');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onClearStorageDialogClosed_() {
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#clearStorage');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-details': SiteDetailsElement;
  }
}

customElements.define(SiteDetailsElement.is, SiteDetailsElement);
