// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import 'chrome://resources/cr_components/iph_bubble/iph_bubble.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../site_settings/settings_category_default_radio_group.js';
import '../site_settings/site_data_details_subpage.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared_css.js';

import {IPHBubbleElement} from 'chrome://resources/cr_components/iph_bubble/iph_bubble.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SyncStatus} from '../people_page/sync_browser_proxy.js';
import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';
import {ChooserType, ContentSettingsTypes, NotificationSetting} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.js';

type BlockAutoplayStatus = {
  enabled: boolean,
  pref: chrome.settingsPrivate.PrefObject,
};

type FocusConfig = Map<string, (string|(() => void))>;

export interface SettingsPrivacyPageElement {
  $: {
    clearBrowsingData: CrLinkRowElement,
    cookiesLinkRow: CrLinkRowElement,
    iphBubble: IPHBubbleElement,
    permissionsLinkRow: CrLinkRowElement,
    privacySandboxLinkRow: CrLinkRowElement,
    securityLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacyPageElementBase =
    RouteObserverMixin(WebUIListenerMixin(
        I18nMixin(PrefsMixin(BaseMixin(PolymerElement))))) as {
      new ():
          PolymerElement & I18nMixinInterface & WebUIListenerMixinInterface &
      PrefsMixinInterface & RouteObserverMixinInterface
    };

export class SettingsPrivacyPageElement extends SettingsPrivacyPageElementBase {
  static get is() {
    return 'settings-privacy-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        }
      },

      showClearBrowsingDataDialog_: Boolean,

      enableSafeBrowsingSubresourceFilter_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
        }
      },

      cookieSettingDescription_: String,

      enableBlockAutoplayContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
        }
      },

      blockAutoplayStatus_: {
        type: Object,
        value() {
          return {};
        }
      },

      enablePaymentHandlerContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
        }
      },

      enableExperimentalWebPlatformFeatures_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableExperimentalWebPlatformFeatures');
        },
      },

      enableSecurityKeysSubpage_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysSubpage');
        }
      },

      enableQuietNotificationPromptsSetting_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableQuietNotificationPromptsSetting'),
      },

      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },

      enablePrivacyReview_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privacyReviewEnabled'),
      },

      enableIphDemo_: {
        reflectToAttribute: true,
        type: Boolean,
        value: () => loadTimeData.getBoolean('iphDemoEnabled'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (routes.SECURITY) {
            map.set(routes.SECURITY.path, '#securityLinkRow');
          }

          if (routes.COOKIES) {
            map.set(
                `${routes.COOKIES.path}_${routes.PRIVACY.path}`,
                '#cookiesLinkRow');
            map.set(
                `${routes.COOKIES.path}_${routes.BASIC.path}`,
                '#cookiesLinkRow');
          }

          if (routes.SITE_SETTINGS) {
            map.set(routes.SITE_SETTINGS.path, '#permissionsLinkRow');
          }

          if (routes.PRIVACY_REVIEW) {
            map.set(routes.PRIVACY_REVIEW.path, '#privacyReviewLinkRow');
          }

          return map;
        },
      },

      /**
       * Expose NotificationSetting enum to HTML bindings.
       */
      notificationSettingEnum_: {
        type: Object,
        value: NotificationSetting,
      },

      searchFilter_: String,
      siteDataFilter_: String,

      /**
       * Expose ContentSettingsTypes enum to HTML bindings.
       */
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      /**
       * Expose ChooserType enum to HTML bindings.
       */
      chooserTypeEnum_: {
        type: Object,
        value: ChooserType,
      },
    };
  }

  private isGuest_: boolean;
  private showClearBrowsingDataDialog_: boolean;
  private enableSafeBrowsingSubresourceFilter_: boolean;
  private cookieSettingDescription_: string;
  private enableBlockAutoplayContentSetting_: boolean;
  private blockAutoplayStatus_: BlockAutoplayStatus;
  private enablePaymentHandlerContentSetting_: boolean;
  private enableExperimentalWebPlatformFeatures_: boolean;
  private enableSecurityKeysSubpage_: boolean;
  private enableQuietNotificationPromptsSetting_: boolean;
  private enableWebBluetoothNewPermissionsBackend_: boolean;
  private enablePrivacyReview_: boolean;
  private focusConfig_: FocusConfig;
  private searchFilter_: string;
  private siteDataFilter_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  ready() {
    super.ready();

    this.onBlockAutoplayStatusChanged_({
      pref: {
        key: '',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      enabled: false
    });

    this.addWebUIListener(
        'onBlockAutoplayStatusChanged',
        (status: BlockAutoplayStatus) =>
            this.onBlockAutoplayStatusChanged_(status));

    SiteSettingsPrefsBrowserProxyImpl.getInstance()
        .getCookieSettingDescription()
        .then(
            (description: string) => this.cookieSettingDescription_ =
                description);
    this.addWebUIListener(
        'cookieSettingDescriptionChanged',
        (description: string) => this.cookieSettingDescription_ = description);

    this.addWebUIListener(
        'is-managed-changed', this.onIsManagedChanged_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
  }

  currentRouteChanged() {
    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
  }

  /**
   * Called when the block autoplay status changes.
   */
  private onBlockAutoplayStatusChanged_(autoplayStatus: BlockAutoplayStatus) {
    this.blockAutoplayStatus_ = autoplayStatus;
  }

  /**
   * Updates the block autoplay pref when the toggle is changed.
   */
  private onBlockAutoplayToggleChange_(event: Event) {
    const target = event.target as SettingsToggleButtonElement;
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
  }

  /**
   * This is a workaround to connect the remove all button to the subpage.
   */
  private onRemoveAllCookiesFromSite_() {
    // Intentionally not casting to SiteDataDetailsSubpageElement, as this would
    // require importing site_data_details_subpage.js and would endup in the
    // main JS bundle.
    const node = this.shadowRoot!.querySelector('site-data-details-subpage');
    if (node) {
      node.removeAll();
    }
  }

  private onShowIPHBubbleTap_() {
    this.interactedWithPage_();
    if (this.$.iphBubble.open) {
      this.$.iphBubble.hide();
    } else {
      this.$.iphBubble.show();
    }
  }

  private onClearBrowsingDataTap_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
  }

  private onCookiesClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.COOKIES);
  }

  private onDialogClosed_() {
    Router.getInstance().navigateTo(assert(routes.CLEAR_BROWSER_DATA.parent!));
    setTimeout(() => {
      // Focus after a timeout to ensure any a11y messages get read before
      // screen readers read out the newly focused element.
      focusWithoutInk(
          assert(this.shadowRoot!.querySelector('#clearBrowsingData')!));
    });
  }

  private onPermissionsPageClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
  }

  private onSecurityPageClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromParentSettings');
    Router.getInstance().navigateTo(routes.SECURITY);
  }

  private onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenedFromSettingsParent');
    // Create a MouseEvent directly to avoid Polymer failing to synthesise a
    // click event if this function was called in response to a touch event.
    // See crbug.com/1253883 for details.
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>('#privacySandboxLink')!
        .dispatchEvent(new MouseEvent('click'));
  }

  private onPrivacyReviewClick_() {
    // TODO(crbug/1215630): Implement metrics.
    Router.getInstance().navigateTo(
        routes.PRIVACY_REVIEW, /* dynamicParams */ undefined,
        /* removeSearch */ true);
  }

  private onIsManagedChanged_(isManaged: boolean) {
    // If the user became managed, then hide the privacy review entry point.
    // However, if the user was managed before and is no longer now, then do not
    // make the privacy review entry point visible, as the Settings route for
    // privacy review would still be unavailable until the page is reloaded.
    this.enablePrivacyReview_ = this.enablePrivacyReview_ && !isManaged;
  }

  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    // If the user signed in to a child user account, then hide the privacy
    // review entry point. However, if the user was a child user before and is
    // no longer now then do not make the privacy review entry point visible, as
    // the Settings route for privacy review would still be unavailable until
    // the page is reloaded.
    this.enablePrivacyReview_ =
        this.enablePrivacyReview_ && !syncStatus.childUser;
  }

  private interactedWithPage_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.USED_PRIVACY_CARD);
  }

  private computePrivacySandboxSublabel_(): string {
    return this.getPref('privacy_sandbox.apis_enabled').value ?
        this.i18n('privacySandboxTrialsEnabled') :
        this.i18n('privacySandboxTrialsDisabled');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page': SettingsPrivacyPageElement;
  }
}

customElements.define(
    SettingsPrivacyPageElement.is, SettingsPrivacyPageElement);
