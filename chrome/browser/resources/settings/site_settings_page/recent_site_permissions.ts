// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {CrTooltipIconElement} from 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {FocusConfig} from '../focus_config.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {ContentSettingsTypes} from '../site_settings/constants.js';
import {AllSitesAction2, ContentSetting, SiteSettingSource} from '../site_settings/constants.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';
import type {RawSiteException, RecentSitePermissions} from '../site_settings/site_settings_prefs_browser_proxy.js';
import {TooltipMixin} from '../tooltip_mixin.js';

import {getTemplate} from './recent_site_permissions.html.js';
import {getLocalizationStringForContentType} from './site_settings_page_util.js';

export interface SettingsRecentSitePermissionsElement {
  $: {
    tooltip: CrTooltipElement,
  };
}

const SettingsRecentSitePermissionsElementBase =
    TooltipMixin(RouteObserverMixin(
        SiteSettingsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsRecentSitePermissionsElement extends
    SettingsRecentSitePermissionsElementBase {
  static get is() {
    return 'settings-recent-site-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      noRecentPermissions: {
        type: Boolean,
        computed: 'computeNoRecentPermissions_(recentSitePermissionsList_)',
        notify: true,
      },

      shouldFocusAfterPopulation_: Boolean,

      /**
       * List of recent site permissions grouped by source.
       */
      recentSitePermissionsList_: {
        type: Array,
        value: () => [],
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },
    };
  }

  noRecentPermissions: boolean;
  private shouldFocusAfterPopulation_: boolean;
  private recentSitePermissionsList_: RecentSitePermissions[];
  focusConfig: FocusConfig;
  private lastSelected_: {origin: string, incognito: boolean, index: number}|
      null;

  constructor() {
    super();

    /**
     * When navigating to a site details sub-page, |lastSelected_| holds the
     * origin and incognito bit associated with the link that sent the user
     * there, as well as the index in recent permission list for that entry.
     * This allows for an intelligent re-focus upon a back navigation.
     */
    this.lastSelected_ = null;
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    this.focusConfig.set(
        routes.SITE_SETTINGS_SITE_DETAILS.path + '_' +
            routes.SITE_SETTINGS.path,
        () => {
          this.shouldFocusAfterPopulation_ = true;
        });
  }

  /**
   * Reload the site recent site permission list whenever the user navigates
   * to the site settings page.
   */
  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute.path === routes.SITE_SETTINGS.path) {
      this.populateList_();
    }
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'onIncognitoStatusChanged',
        (hasIncognito: boolean) =>
            this.onIncognitoStatusChanged_(hasIncognito));
    this.browserProxy.updateIncognitoStatus();
  }

  /**
   * @return a user-friendly name for the origin a set of recent permissions
   *     is associated with.
   */
  private getDisplayName_(recentSitePermissions: RecentSitePermissions):
      string {
    return recentSitePermissions.displayName;
  }

  /**
   * @return the site scheme for the origin of a set of recent permissions.
   */
  private getSiteScheme_({origin}: RecentSitePermissions): string {
    const scheme = this.toUrl(origin)!.protocol.slice(0, -1);
    return scheme === 'https' ? '' : scheme;
  }

  /**
   * @return the display text which describes the set of recent permissions.
   */
  private getPermissionsText_({recentPermissions}: RecentSitePermissions):
      string {
    // Recently changed permisisons for a site are grouped into three buckets,
    // each described by a single sentence.
    const groupSentences = [
      this.getPermissionGroupText_(
          'Allowed',
          recentPermissions.filter(
              exception => exception.setting === ContentSetting.ALLOW)),
      this.getPermissionGroupText_(
          'AutoBlocked',
          recentPermissions.filter(
              exception => exception.source === SiteSettingSource.EMBARGO)),
      this.getPermissionGroupText_(
          'Blocked',
          recentPermissions.filter(
              exception => exception.setting === ContentSetting.BLOCK &&
                  exception.source !== SiteSettingSource.EMBARGO)),
    ].filter(string => string.length > 0);

    let finalText = '';
    // The final text may be composed of multiple sentences, so may need the
    // appropriate sentence separators.
    for (const sentence of groupSentences) {
      if (finalText.length > 0) {
        // Whitespace is a valid sentence separator w.r.t i18n.
        finalText += `${this.i18n('sentenceEnd')} ${sentence}`;
      } else {
        finalText = sentence;
      }
    }
    if (groupSentences.length > 1) {
      finalText += this.i18n('sentenceEnd');
    }
    return finalText;
  }

  /**
   * @return the display sentence which groups the provided |exceptions|
   *    together and applies the appropriate description based on |setting|.
   */
  private getPermissionGroupText_(
      setting: string, exceptions: RawSiteException[]): string {
    if (exceptions.length === 0) {
      return '';
    }

    const typeStrings = exceptions.map(exception => {
      const localizationString = getLocalizationStringForContentType(
          exception.type as ContentSettingsTypes);
      return localizationString ? this.i18n(localizationString) : '';
    });

    if (exceptions.length === 1) {
      return this.i18n(`recentPermission${setting}OneItem`, ...typeStrings);
    }
    if (exceptions.length === 2) {
      return this.i18n(`recentPermission${setting}TwoItems`, ...typeStrings);
    }

    return this.i18n(
        `recentPermission${setting}MoreThanTwoItems`, typeStrings[0],
        exceptions.length - 1);
  }

  /**
   * @return the correct CSS class to apply depending on this recent site
   *     permissions entry based on the index.
   */
  private getClassForIndex_(index: number): string {
    return index === 0 ? 'first' : '';
  }

  /**
   * @return true if there are no recent site permissions to display
   */
  private computeNoRecentPermissions_(): boolean {
    return this.recentSitePermissionsList_.length === 0;
  }

  /**
   * Called for when incognito is enabled or disabled. Only called on change
   * (opening N incognito windows only fires one message). Another message is
   * sent when the *last* incognito window closes.
   */
  private onIncognitoStatusChanged_(hasIncognito: boolean) {
    // We're only interested in the case where we transition out of incognito
    // and we are currently displaying an incognito entry.
    if (hasIncognito === false &&
        this.recentSitePermissionsList_.some(p => p.incognito)) {
      this.populateList_();
    }
  }

  /**
   * A handler for selecting a recent site permissions entry.
   */
  private onRecentSitePermissionClick_(
      e: DomRepeatEvent<RecentSitePermissions>) {
    const origin = this.recentSitePermissionsList_[e.model.index].origin;
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS, new URLSearchParams({site: origin}));
    this.browserProxy.recordAction(AllSitesAction2.ENTER_SITE_DETAILS);
    this.lastSelected_ = {
      index: e.model.index,
      origin: e.model.item.origin,
      incognito: e.model.item.incognito,
    };
  }

  private onShowIncognitoTooltip_(e: Event) {
    e.stopPropagation();

    this.showTooltipAtTarget(this.$.tooltip, e.target! as Element);
  }

  /**
   * Called after the list has finished populating and |lastSelected_| contains
   * a valid entry that should attempt to be focused. If lastSelected_ cannot
   * be found the index where it used to be is focused. This may result in
   * focusing another link arrow, or an incognito information icon. If the
   * recent permission list is empty, focus is lost.
   */
  private focusLastSelected_() {
    if (this.noRecentPermissions) {
      return;
    }
    const currentIndex =
        this.recentSitePermissionsList_.findIndex((permissions) => {
          return permissions.origin === this.lastSelected_!.origin &&
              permissions.incognito === this.lastSelected_!.incognito;
        });

    const fallbackIndex = Math.min(
        this.lastSelected_!.index, this.recentSitePermissionsList_.length - 1);

    const index = currentIndex > -1 ? currentIndex : fallbackIndex;

    if (this.recentSitePermissionsList_[index].incognito) {
      const icon = this.shadowRoot!.querySelector<CrTooltipIconElement>(
          `#incognitoInfoIcon_${index}`);
      assert(!!icon);
      const toFocus = icon.getFocusableElement() as HTMLElement;
      assert(!!toFocus);
      focusWithoutInk(toFocus);
    } else {
      const toFocus = this.shadowRoot!.querySelector<HTMLElement>(
          `#siteEntryButton_${index}`);
      assert(!!toFocus);
      focusWithoutInk(toFocus);
    }
  }

  /**
   * Retrieve the list of recently changed permissions and implicitly trigger
   * the update of the display list.
   */
  private async populateList_() {
    this.recentSitePermissionsList_ =
        await this.browserProxy.getRecentSitePermissions(3);
  }

  /**
   * Called when the dom-repeat DOM has changed. This allows updating the
   * focused element after the elements have been adjusted.
   */
  private onDomChange_() {
    if (this.shouldFocusAfterPopulation_) {
      this.focusLastSelected_();
      this.shouldFocusAfterPopulation_ = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-recent-site-permissions': SettingsRecentSitePermissionsElement;
  }
}

customElements.define(
    SettingsRecentSitePermissionsElement.is,
    SettingsRecentSitePermissionsElement);
