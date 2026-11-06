// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list-entry' shows an Allowed and Blocked site for a given category.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import '../icons.html.js';
import '../site_favicon.js';

import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {ChooserType, ContentSetting, ContentSettingsTypes, CookiesExceptionType, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getCss} from './site_list_entry.css.js';
import {getHtml} from './site_list_entry.html.js';
import type {SiteException} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface SiteListEntryElement {
  $: {
    siteDescription: HTMLElement,
  };
}

const SiteListEntryElementBase =
    FocusRowMixinLit(SiteSettingsMixinLit(I18nMixinLit(CrLitElement)));

export class SiteListEntryElement extends SiteListEntryElementBase {
  static get is() {
    return 'site-list-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       */
      readOnlyList: {type: Boolean},

      /** Site to display in the widget.*/
      model: {type: Object},

      /** Whether this entry is the only entry in the Allowed/Blocked section.*/
      singletonEntry: {type: Boolean},

      /**
       * The header for the Allowed/Blocked section this entry is in. Used for
       * aria-label to fix cases where the screenreader hasn't already read it.
       */
      sectionHeader: {type: String},

      /**
       * If the site represented is part of a chooser exception, the chooser
       * type will be stored here to allow the permission to be manipulated.
       */
      chooserType: {type: String},

      /**
       * If the site represented is part of a chooser exception, the chooser
       * object will be stored here to allow the permission to be manipulated.
       */
      chooserObject: {type: Object},

      allowNavigateToSiteDetail_: {type: Boolean},

      /**
       * Type of cookies exceptions based on the use of wildcard in the
       * patterns. See `CookiesExceptionType`.
       */
      cookiesExceptionType: {type: String},
    };
  }

  accessor readOnlyList: boolean = false;
  accessor model: SiteException = {
    category: ContentSettingsTypes.COOKIES,
    embeddingOrigin: '',
    incognito: false,
    isEmbargoed: false,
    origin: '',
    displayName: '',
    setting: ContentSetting.DEFAULT,
    enforcement: null,
    controlledBy: chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
  };
  accessor singletonEntry: boolean = false;
  accessor sectionHeader: string = '';
  accessor chooserType: ChooserType = ChooserType.NONE;
  accessor chooserObject: object|null = null;
  protected accessor allowNavigateToSiteDetail_: boolean = false;
  accessor cookiesExceptionType: CookiesExceptionType|undefined = undefined;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('model')) {
      if (this.model.origin) {
        this.browserProxy.isOriginValid(this.model.origin).then(valid => {
          this.allowNavigateToSiteDetail_ = valid;
        });
      } else {
        this.allowNavigateToSiteDetail_ = false;
      }
    }

    // TODO(crbug.com/393471368): Remove once <site-list> no longer renders this
    // element inside a Polymer <iron-list>. The row height depends on whether
    // a site description is shown, and <iron-list> only re-measures rows when
    // it receives 'iron-resize'.
    if (changedProperties.has('model') ||
        changedProperties.has('cookiesExceptionType')) {
      this.fire('iron-resize');
    }
  }

  /**
   * <cr-policy-pref-indicator> only reads the |enforcement| and |controlledBy|
   * fields, which SiteException shares with PrefObject.
   */
  protected getPolicyPref_(): chrome.settingsPrivate.PrefObject {
    return {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
      enforcement: this.model.enforcement ?? undefined,
      controlledBy: this.model.controlledBy,
    };
  }

  protected onShowTooltipMouseenter_() {
    this.showTooltip_();
  }

  protected onShowTooltipFocus_() {
    this.showTooltip_();
  }

  private showTooltip_() {
    const indicator = this.shadowRoot.querySelector('cr-policy-pref-indicator');
    assert(indicator);
    // The tooltip text is used by an cr-tooltip contained inside the
    // cr-policy-pref-indicator. This text is needed here to send up to the
    // common tooltip component.
    const text = indicator.indicatorTooltip;
    this.fire('show-tooltip', {target: indicator, text});
  }

  protected onShowIncognitoTooltipMouseenter_() {
    this.showIncognitoTooltip_();
  }

  protected onShowIncognitoTooltipFocus_() {
    this.showIncognitoTooltip_();
  }

  private showIncognitoTooltip_() {
    const tooltip = this.shadowRoot.querySelector('#incognitoTooltip');
    assert(tooltip);
    // The incognito <cr-tooltip-icon> does not show its own tooltip. Its text
    // is sent up to the common tooltip component instead.
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

  protected shouldShowResetButton_(): boolean {
    return this.model.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !this.isUserEditable_();
  }

  protected shouldShowActionMenu_(): boolean {
    return this.model.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        this.isUserEditable_();
  }

  /**
   * A handler for selecting a site (by clicking on the origin).
   */
  protected onOriginClick_() {
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
  protected getDisplayName_(): string {
    if (this.model.embeddingOrigin &&
        this.model.category === ContentSettingsTypes.COOKIES &&
        this.model.origin.trim() === SITE_EXCEPTION_WILDCARD) {
      return this.model.embeddingOrigin;
    }
    return this.model.displayName;
  }

  /**
   * Returns an appropriate aria-label for this entry. The reset (trash can)
   * button has a different aria-label, see the HTML.
   */
  protected getAriaLabel_(): string {
    if (this.singletonEntry) {
      // Screen readers sometimes only read the grid's label (which is in
      // site_list.html) if the grid has more than 1 row. That contains the
      // important context of whether this site is allowed or blocked, so we
      // explicitly add it to the aria-label here to make sure it's read.
      return `${this.getDisplayName_()} ${this.sectionHeader}`;
    }
    return this.getDisplayName_();
  }

  /**
   * Returns an appropriate aria-label for the remove button. This aria-label
   * clearly indicates a site name and section header. It should help the
   * screenreader user understand that this button will remove the site from the
   * list. There are two lists, one for allowed sites and one for blocked sites.
   */
  protected getRemoveButtonAriaLabel_(): string {
    return this.i18n(
        'siteSettingsActionResetFromListA11y', this.getDisplayName_(),
        this.sectionHeader);
  }

  /**
   * Returns an appropriate aria-label for the view button. This aria-label
   * clearly indicates a site name and section header. It should help the
   * screenreader user understand that this button will open a subpage for the
   * site from the list. There are two lists, one for allowed sites and one for
   * blocked sites.
   */
  protected getViewButtonAriaLabel_(): string {
    return this.i18n(
        'siteSettingsActionViewFromListA11y', this.getDisplayName_(),
        this.sectionHeader);
  }

  /**
   * Returns the appropriate origin that a favicon will be fetched for.
   */
  protected getFaviconOrigin_(): string {
    if (this.model.origin.trim() !== SITE_EXCEPTION_WILDCARD &&
        this.model.origin.trim() !== '') {
      return this.model.origin.trim();
    }
    if (this.model.embeddingOrigin.trim() !== SITE_EXCEPTION_WILDCARD &&
        this.model.embeddingOrigin.trim() !== '') {
      return this.model.embeddingOrigin.trim();
    }
    return '';
  }

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' string, or a third-party exception
   * description string.
   */
  protected getSiteDescription_(): string {
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
      } else {
        description = loadTimeData.getStringF(
            'embeddedOnHost', this.sanitizePort(this.model.embeddingOrigin));
      }
    }

    try {
      if (this.model.origin) {
        const url = new URL(this.model.origin);
        if (url.protocol === 'chrome-extension:') {
          description = loadTimeData.getStringF(
              'siteSettingsExtensionIdDescription', url.hostname);
        }
      }
    } finally {
      return description;
    }
  }

  protected shouldShowPolicyPrefIndicator_(): boolean {
    return this.model.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !!this.model.controlledBy;
  }

  protected onResetButtonClick_() {
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

  protected onShowActionMenuClick_() {
    // Chooser exceptions do not support the action menu, so do nothing.
    if (this.chooserType !== ChooserType.NONE) {
      return;
    }

    this.fire('show-action-menu', {
      anchor: this.shadowRoot.querySelector('#actionMenuButton'),
      model: this.model,
    });
  }

  protected getActionMenuButtonLabel_(): string {
    return this.i18n(
        'siteDataPageAddSiteContextMenuLabel', this.getDisplayName_());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-list-entry': SiteListEntryElement;
  }
}

customElements.define(SiteListEntryElement.is, SiteListEntryElement);
