// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {DomRepeatEvent, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {Route, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes, NotificationSetting} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

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
      // The prefs object is only populated for the instance of this element
      // which contains the notifications link row, avoiding non-actionable
      // firing of the observer.
      'updateNotificationsLabel_(prefs.generated.notification.*)',
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

    const hasCookies = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.COOKIES;
    });
    if (hasCookies) {
      // The cookies sub-label is provided by an update from C++.
      this.browserProxy_.getCookieSettingDescription().then(
          (label: string) => this.updateCookiesLabel_(label));
      this.addWebUiListener(
          'cookieSettingDescriptionChanged',
          (label: string) => this.updateCookiesLabel_(label));
    }
  }

  /**
   * @param category The category to refresh (fetch current value + update UI)
   * @return A promise firing after the label has been updated.
   */
  private refreshDefaultValueLabel_(category: ContentSettingsTypes):
      Promise<void> {
    // Default labels are not applicable to ZOOM_LEVELS, PDF, PROTECTED_CONTENT,
    // or SITE_DATA.
    if (category === ContentSettingsTypes.ZOOM_LEVELS ||
        category === ContentSettingsTypes.PROTECTED_CONTENT ||
        category === ContentSettingsTypes.PDF_DOCUMENTS ||
        category === ContentSettingsTypes.SITE_DATA) {
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.COOKIES) {
      // Updates to the cookies label are handled by the
      // cookieSettingDescriptionChanged event listener.
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.NOTIFICATIONS) {
      // Updates to the notifications label are handled by a preference
      // observer.
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
   * Update the notifications link row label when the notifications setting
   * description changes.
   */
  private updateNotificationsLabel_() {
    const state = this.getPref('generated.notification').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.NOTIFICATIONS);

    // updateNotificationsLabel_ should only be called for the
    // site-settings-list instance which contains notifications.
    assert(index !== -1);

    let label = 'siteSettingsNotificationsBlocked';
    if (state === NotificationSetting.ASK) {
      label = 'siteSettingsNotificationsAllowed';
    } else if (state === NotificationSetting.QUIETER_MESSAGING) {
      label = 'siteSettingsNotificationsPartial';
    }
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  private onClick_(event: DomRepeatEvent<CategoryListItem>) {
    Router.getInstance().navigateTo(this.categoryList[event.model.index].route);
  }
}

customElements.define(
    SettingsSiteSettingsListElement.is, SettingsSiteSettingsListElement);
