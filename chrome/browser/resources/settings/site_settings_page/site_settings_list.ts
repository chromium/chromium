// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode, SettingsState} from '../site_settings/constants.js';
import type {SiteSettingsPrefsBrowserProxy} from '../site_settings/site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {getTemplate} from './site_settings_list.html.js';

export interface CategoryListItem {
  route: Route;
  id: ContentSettingsTypes;
  label: string;
  icon?: string;
  enabledLabel?: string;
  disabledLabel?: string;
  otherLabel?: string;
  shouldShow?: () => boolean;
}

export function defaultSettingLabel(
    setting: string, enabled: string, disabled: string,
    other?: string): string {
  if (setting === ContentSetting.BLOCK) {
    return disabled;
  }
  if (setting === ContentSetting.ALLOW) {
    return enabled;
  }

  return other || enabled;
}


const SettingsSiteSettingsListElementBase =
    PrefsMixin(BaseMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

class SettingsSiteSettingsListElement extends
    SettingsSiteSettingsListElementBase {
  static get is() {
    return 'settings-site-settings-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      categoryList: Array,

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },
    };
  }

  static get observers() {
    return [
      'updateNotificationsLabel_(prefs.generated.notification.*)',
      'updateLocationLabel_(prefs.generated.geolocation.*)',
      'updateSiteDataLabel_(prefs.generated.cookie_default_content_setting.*)',
      'updateThirdPartyCookiesLabel_(prefs.profile.cookie_controls_mode.*,' +
          'prefs.tracking_protection.block_all_3pc_toggle_enabled.*)',
      'updateOfferWritingHelpLabel_(prefs.compose.proactive_nudge_enabled.*)',
    ];
  }

  categoryList: CategoryListItem[];
  focusConfig: FocusConfig;
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    for (const item of this.categoryList) {
      this.focusConfig.set(item.route.path, () => microTask.run(() => {
        const toFocus =
            this.shadowRoot!.querySelector<HTMLElement>(`#${item.id}`);
        assert(!!toFocus);
        focusWithoutInk(toFocus);
      }));
    }
  }

  override ready() {
    super.ready();

    Promise
        .all(this.categoryList.map(
            item => this.refreshDefaultValueLabel_(item.id)))
        .then(() => {
          this.fire('site-settings-list-labels-updated-for-testing');
        });

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) =>
            this.refreshDefaultValueLabel_(category));

    const hasProtocolHandlers = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.PROTOCOL_HANDLERS;
    });

    if (hasProtocolHandlers) {
      // The protocol handlers have a separate enabled/disabled notifier.
      this.addWebUiListener('setHandlersEnabled', (enabled: boolean) => {
        this.updateDefaultValueLabel_(
            ContentSettingsTypes.PROTOCOL_HANDLERS,
            enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK);
      });
      this.browserProxy_.observeProtocolHandlersEnabledState();
    }
  }

  /**
   * @param category The category to refresh (fetch current value + update UI)
   * @return A promise firing after the label has been updated.
   */
  private refreshDefaultValueLabel_(category: ContentSettingsTypes):
      Promise<void> {
    // Default labels are not applicable to ZOOM_LEVELS, PDF, PROTECTED_CONTENT,
    // SITE_DATA, or OFFER_WRITING_HELP.
    if (category === ContentSettingsTypes.ZOOM_LEVELS ||
        category === ContentSettingsTypes.PROTECTED_CONTENT ||
        category === ContentSettingsTypes.PDF_DOCUMENTS ||
        category === ContentSettingsTypes.SITE_DATA ||
        category === ContentSettingsTypes.OFFER_WRITING_HELP) {
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.COOKIES) {
      if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled') &&
          loadTimeData.getBoolean('isTrackingProtectionUxEnabled')) {
        const index = this.categoryList.map(e => e.id).indexOf(
            ContentSettingsTypes.COOKIES);
        this.set(
            `categoryList.${index}.subLabel`,
            this.i18n('trackingProtectionLinkRowSubLabel'));
      }
      // Updates to the cookies label are handled by the
      // cookieSettingDescriptionChanged event listener.
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.NOTIFICATIONS) {
      // Updates to the notifications label are handled by a preference
      // observer.
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.PERFORMANCE) {
      const index = this.categoryList.map(e => e.id).indexOf(
          ContentSettingsTypes.PERFORMANCE);
      this.set(
          `categoryList.${index}.subLabel`,
          this.i18n('siteSettingsPerformanceSublabel'));
      return Promise.resolve();
    }

    return this.browserProxy_.getDefaultValueForContentType(category).then(
        defaultValue => {
          this.updateDefaultValueLabel_(category, defaultValue.setting);
        });
  }

  /**
   * Updates the DOM for the given |category| to display a label that
   * corresponds to the given |setting|.
   */
  private updateDefaultValueLabel_(
      category: ContentSettingsTypes, setting: ContentSetting) {
    const element = this.$$<HTMLElement>(`#${category}`);
    if (!element) {
      // |category| is not part of this list.
      return;
    }

    const index =
        this.shadowRoot!.querySelector('dom-repeat')!.indexForElement(element);
    const dataItem = this.categoryList[index!];
    this.set(
        `categoryList.${index}.subLabel`,
        defaultSettingLabel(
            setting,
            dataItem.enabledLabel ? this.i18n(dataItem.enabledLabel) : '',
            dataItem.disabledLabel ? this.i18n(dataItem.disabledLabel) : '',
            dataItem.otherLabel ? this.i18n(dataItem.otherLabel) : undefined));
  }

  /**
   * Update the cookies link row label when the cookies setting description
   * changes.
   */
  private updateCookiesLabel_(label: string) {
    const index = this.shadowRoot!.querySelector('dom-repeat')!.indexForElement(
        this.shadowRoot!.querySelector('#cookies')!);
    this.set(`categoryList.${index}.subLabel`, label);
  }

  /**
   * Update the geolocation link row label when the geolocation setting
   * description changes.
   */
  private updateLocationLabel_() {
    const state = this.getPref('generated.geolocation').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.GEOLOCATION);

    // The location row might not be part of the current site-settings-list
    // but the class always observes the preference.
    if (index === -1) {
      return;
    }

    let label = 'siteSettingsLocationBlocked';
    if (state === SettingsState.LOUD) {
      label = 'siteSettingsLocationAskLoud';
    } else if (state === SettingsState.QUIET) {
      label = 'siteSettingsLocationAskQuiet';
    } else if (state === SettingsState.CPSS) {
      label = 'siteSettingsLocationAskCPSS';
    }
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }


  /**
   * Update the notifications link row label when the notifications setting
   * description changes.
   */
  private updateNotificationsLabel_() {
    const state = this.getPref('generated.notification').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.NOTIFICATIONS);

    // The notification row might not be part of the current site-settings-list
    // but the class always observes the preference.
    if (index === -1) {
      return;
    }

    let label = 'siteSettingsNotificationsBlocked';
    if (state === SettingsState.LOUD) {
      label = 'siteSettingsNotificationsAskLoud';
    } else if (state === SettingsState.QUIET) {
      label = 'siteSettingsNotificationsAskQuiet';
    } else if (state === SettingsState.CPSS) {
      label = 'siteSettingsNotificationsAskCPSS';
    }
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  /**
   * Update the site data link row label when the default cookies content
   * setting changes.
   */
  private updateSiteDataLabel_() {
    const state =
        this.getPref('generated.cookie_default_content_setting').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.SITE_DATA);

    // The site data row might not be part of the current site-settings-list
    // but the class always observes the preference.
    if (index === -1) {
      return;
    }

    let label;
    if (state === ContentSetting.ALLOW) {
      label = 'siteSettingsSiteDataAllowedSubLabel';
    } else if (state === ContentSetting.SESSION_ONLY) {
      label = 'siteSettingsSiteDataDeleteOnExitSubLabel';
    } else if (state === ContentSetting.BLOCK) {
      label = 'siteSettingsSiteDataBlockedSubLabel';
    }
    assert(!!label);
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  /**
   * Update the third-party cookies link row label when the pref changes.
   */
  private updateThirdPartyCookiesLabel_() {
    if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled') &&
        loadTimeData.getBoolean('isTrackingProtectionUxEnabled')) {
      return;
    }

    const index =
        this.categoryList.map(e => e.id).indexOf(ContentSettingsTypes.COOKIES);
    // The third-party cookies might not be part of the current
    // site-settings-list but the class always observes the preference.
    if (index === -1) {
      return;
    }

    let label;
    if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
      if (this.getPref('tracking_protection.block_all_3pc_toggle_enabled')
              .value) {
        label = 'thirdPartyCookiesLinkRowSublabelDisabled';
      } else {
        label = 'thirdPartyCookiesLinkRowSublabelLimited';
      }
    } else {
      const state = this.getPref('profile.cookie_controls_mode').value;
      if (state === CookieControlsMode.OFF) {
        label = 'thirdPartyCookiesLinkRowSublabelEnabled';
      } else if (state === CookieControlsMode.INCOGNITO_ONLY) {
        label = 'thirdPartyCookiesLinkRowSublabelDisabledIncognito';
      } else if (state === CookieControlsMode.BLOCK_THIRD_PARTY) {
        label = 'thirdPartyCookiesLinkRowSublabelDisabled';
      }
    }

    assert(!!label);
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  private updateOfferWritingHelpLabel_() {
    if (!loadTimeData.getBoolean('enableComposeProactiveNudge')) {
      return;
    }

    const enabled = this.getPref('compose.proactive_nudge_enabled').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.OFFER_WRITING_HELP);

    // The writing help data row might not be part of the current
    // site-settings-list but the class always observes the preference.
    if (index === -1) {
      return;
    }

    const label = enabled ? 'siteSettingsOfferWritingHelpEnabledSublabel' :
                            'siteSettingsOfferWritingHelpDisabledSublabel';
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  private onClick_(event: DomRepeatEvent<CategoryListItem>) {
    Router.getInstance().navigateTo(this.categoryList[event.model.index].route);
  }
}

customElements.define(
    SettingsSiteSettingsListElement.is, SettingsSiteSettingsListElement);
