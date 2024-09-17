// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list-entry' shows an Allowed and Blocked site for a given category.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {ChooserType, ContentSettingsTypes, CookiesExceptionType, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getTemplate} from './site_list_entry.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {SiteException} from './site_settings_prefs_browser_proxy.js';

export interface SiteListEntryElement {
  $: {
    actionMenuButton: HTMLElement,
    resetSite: HTMLElement,
  };
}

const SiteListEntryElementBase =
    FocusRowMixin(BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement))));

export class SiteListEntryElement extends SiteListEntryElementBase {
  static get is() {
    return 'site-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       */
      readOnlyList: {
        type: Boolean,
        value: false,
      },

      /**
       * Site to display in the widget.
       */
      model: {
        type: Object,
        observer: 'onModelChanged_',
      },

      /**
       * If the site represented is part of a chooser exception, the chooser
       * type will be stored here to allow the permission to be manipulated.
       */
      chooserType: {
        type: String,
        value: ChooserType.NONE,
      },

      /**
       * If the site represented is part of a chooser exception, the chooser
       * object will be stored here to allow the permission to be manipulated.
       */
      chooserObject: {
        type: Object,
        value: null,
      },

      showPolicyPrefIndicator_: {
        type: Boolean,
        computed: 'computeShowPolicyPrefIndicator_(model)',
      },

      allowNavigateToSiteDetail_: {
        type: Boolean,
        value: false,
      },

      /**
       * Type of cookies exceptions based on the use of wildcard in the
       * patterns. See `CookiesExceptionType`.
       */
      cookiesExceptionType: String,
    };
  }

  private readOnlyList: boolean;
  model: SiteException;
  private chooserType: ChooserType;
  private chooserObject: object;
  private showPolicyPrefIndicator_: boolean;
  private allowNavigateToSiteDetail_: boolean;
  cookiesExceptionType: CookiesExceptionType;

  private onShowTooltip_() {
    const indicator =
        this.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assert(!!indicator);
    // The tooltip text is used by an cr-tooltip contained inside the
    // cr-policy-pref-indicator. This text is needed here to send up to the
    // common tooltip component.
    const text = indicator.indicatorTooltip;
    this.fire('show-tooltip', {target: indicator, text});
  }

  private onShowIncognitoTooltip_() {
    const tooltip = this.shadowRoot!.querySelector('#incognitoTooltip');
    // The tooltip text is used by an cr-tooltip contained inside the
    // cr-policy-pref-indicator. The text is currently held in a private
    // property. This text is needed here to send up to the common tooltip
    // component.
    const text = loadTimeData.getString('incognitoSiteExceptionDesc');
    this.fire('show-tooltip', {target: tooltip, text});
  }

  private isIsolatedWebApp_(): boolean {
    return this.model.origin.startsWith('isolated-app://');
  }

  /**
   * Returns true if this site exception can be edited by the user. Note that
   * this is not the same as readonly; an exception can be removable but not
   * editable.
   */
  private isUserEditable_(): boolean {
    return !this.readOnlyList && !this.model.embeddingOrigin &&
        !this.isIsolatedWebApp_();
  }

  private shouldShowResetButton_(): boolean {
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !this.isUserEditable_();
  }

  private shouldShowActionMenu_(): boolean {
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        this.isUserEditable_();
  }

  /**
   * A handler for selecting a site (by clicking on the origin).
   */
  private onOriginClick_() {
    if (!this.allowNavigateToSiteDetail_) {
      return;
    }
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.model.origin));
  }

  /**
   * Returns the appropriate display name to show for the exception.
   * This can, for example, be the website that is affected itself,
   * or the website whose third parties are also affected.
   */
  private computeDisplayName_(): string {
    if (this.model.embeddingOrigin &&
        ((this.model.category === ContentSettingsTypes.COOKIES &&
          this.model.origin.trim() === SITE_EXCEPTION_WILDCARD) ||
         this.model.category === ContentSettingsTypes.TRACKING_PROTECTION)) {
      return this.model.embeddingOrigin;
    }
    return this.model.displayName;
  }

  /**
   * Returns the appropriate origin that a favicon will be fetched for.
   */
  private computeFaviconOrigin_(): string {
    if (this.model.origin.trim() !== SITE_EXCEPTION_WILDCARD) {
      return this.model.origin.trim();
    }
    if (this.model.embeddingOrigin.trim() !== SITE_EXCEPTION_WILDCARD) {
      return this.model.embeddingOrigin.trim();
    }
    assertNotReached();
  }

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' string, or a third-party exception
   * description string.
   */
  private computeSiteDescription_(): string {
    let description = '';

    // If a description has been set by the handler, have it override others.
    // TODO(crbug.com/40276807): Move all possible descriptions in to this
    // field C++ side so this function can be greatly simplified.
    if (this.model.description) {
      description = this.model.description;
    } else if (this.model.isEmbargoed) {
      assert(
          !this.model.embeddingOrigin,
          'Embedding origin should be empty for embargoed origin.');
      description = loadTimeData.getString('siteSettingsSourceEmbargo');
    } else if (this.model.embeddingOrigin) {
      if (this.model.category === ContentSettingsTypes.COOKIES &&
          this.model.origin.trim() === SITE_EXCEPTION_WILDCARD) {
        // Apply special label only if cookies exceptions are displayed in the
        // mixed list.
        if (this.cookiesExceptionType === CookiesExceptionType.COMBINED) {
          description = loadTimeData.getString(
              'siteSettingsCookiesThirdPartyExceptionLabel');
        }
      } else if (
          this.model.category !== ContentSettingsTypes.TRACKING_PROTECTION) {
        description = loadTimeData.getStringF(
            'embeddedOnHost', this.sanitizePort(this.model.embeddingOrigin));
      }
    }

    try {
      const url = new URL(this.model.origin);
      if (url.protocol === 'chrome-extension:') {
        description = loadTimeData.getStringF(
            'siteSettingsExtensionIdDescription', url.hostname);
      }
    } finally {
      return description;
    }
  }

  private computeShowPolicyPrefIndicator_(): boolean {
    return this.model.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !!this.model.controlledBy;
  }

  private onResetButtonClick_() {
    this.fire('site-list-entry-reset-click');

    // Use the appropriate method to reset a chooser exception.
    if (this.chooserType !== ChooserType.NONE && this.chooserObject !== null) {
      this.browserProxy.resetChooserExceptionForSite(
          this.chooserType, this.model.origin, this.chooserObject);
      return;
    }

    this.browserProxy.resetCategoryPermissionForPattern(
        this.model.origin, this.model.embeddingOrigin, this.model.category,
        this.model.incognito);
  }

  private onShowActionMenuClick_() {
    // Chooser exceptions do not support the action menu, so do nothing.
    if (this.chooserType !== ChooserType.NONE) {
      return;
    }

    this.fire(
        'show-action-menu',
        {anchor: this.$.actionMenuButton, model: this.model});
  }

  private onModelChanged_() {
    if (!this.model) {
      this.allowNavigateToSiteDetail_ = false;
      return;
    }
    this.browserProxy.isOriginValid(this.model.origin).then((valid) => {
      this.allowNavigateToSiteDetail_ = valid;
    });
  }

  private getActionMenuButtonLabel_() {
    return this.i18n(
        'siteDataPageAddSiteContextMenuLabel', this.computeDisplayName_());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-list-entry': SiteListEntryElement;
  }
}

customElements.define(SiteListEntryElement.is, SiteListEntryElement);
