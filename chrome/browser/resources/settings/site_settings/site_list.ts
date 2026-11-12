// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../focus_list_container.js';
import './add_site_dialog.js';
import './edit_exception_dialog.js';
import './site_list_entry.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {SanitizeInnerHtmlOpts} from 'chrome://resources/js/parse_html_subset.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsFocusListContainerElement} from '../focus_list_container.js';
import {TooltipMixinLit} from '../tooltip_mixin_lit.js';

import {ContentSetting, ContentSettingsTypes, CookiesExceptionType, INVALID_CATEGORY_SUBTYPE, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getCss} from './site_list.css.js';
import {getHtml} from './site_list.html.js';
import type {RawSiteException, SiteException, SiteSettingsBrowserProxy} from './site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface SiteListElement {
  $: {
    actionMenu: CrActionMenuElement,
    addSite: HTMLElement,
    category: HTMLElement,
    list: SettingsFocusListContainerElement,
    listContainer: HTMLElement,
    listHeader: HTMLElement,
    systemPermissionWarningText: HTMLElement,
    tooltip: CrTooltipElement,
  };
}

const SiteListElementBase = TooltipMixinLit(
    SiteSettingsMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement))));

export class SiteListElement extends SiteListElementBase {
  static get is() {
    return 'site-list';
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

      categoryHeader: {type: String},

      /**
       * Optional warning message to be displayed bellow the category header.
       */
      systemPermissionWarningKey_: {type: String},

      /**
       * The site serving as the model for the currently open action menu.
       */
      actionMenuSite_: {type: Object},

      /**
       * Whether the "edit exception" dialog should be shown.
       */
      showEditExceptionDialog_: {type: Boolean},

      /**
       * Array of sites to display in the widget.
       */
      sites: {type: Array},

      /**
       * The type of category this widget is displaying data for. Normally
       * either 'allow' or 'block', representing which sites are allowed or
       * blocked respectively.
       */
      categorySubtype: {type: String},

      /**
       * Filters cookies exceptions based on the type (CookiesExceptionType):
       * - THIRD_PARTY: Only show cookies exceptions that have primary pattern
       * as wildcard (third-party cookies exceptions).
       * - SITE_DATA: Only show cookies exceptions that have primary pattern
       * set. This includes site data exceptions (secondary pattern is wildcard)
       * and exceptions with both patterns set (currently possible only via
       * exceptions API).
       * - COMBINED: Doesn't apply any filters, will show exceptions with both
       * pattern types.
       */
      cookiesExceptionType: {type: String},

      hasIncognito_: {type: Boolean},
      showAddSiteDialog_: {type: Boolean},

      /**
       * Whether to show the Allow action in the action menu.
       */
      showAllowAction_: {type: Boolean},

      /**
       * Whether to show the Block action in the action menu.
       */
      showBlockAction_: {type: Boolean},

      /**
       * Whether to show the 'Clear on exit' action in the action
       * menu.
       */
      showSessionOnlyAction_: {type: Boolean},

      tooltipText_: {type: String},
      searchFilter: {type: String},

      /** |sites| filtered by |searchFilter|, as displayed in the list. */
      filteredSites_: {type: Array},
    };
  }

  accessor readOnlyList: boolean = false;
  accessor categoryHeader: string = '';
  private accessor systemPermissionWarningKey_: string|null = null;
  protected accessor actionMenuSite_: SiteException|null = null;
  protected accessor showEditExceptionDialog_: boolean = false;
  accessor sites: SiteException[] = [];
  accessor categorySubtype: ContentSetting|typeof INVALID_CATEGORY_SUBTYPE =
      INVALID_CATEGORY_SUBTYPE;
  accessor cookiesExceptionType: CookiesExceptionType|undefined = undefined;
  protected accessor hasIncognito_: boolean = false;
  protected accessor showAddSiteDialog_: boolean = false;
  protected accessor showAllowAction_: boolean = false;
  protected accessor showBlockAction_: boolean = false;
  private accessor showSessionOnlyAction_: boolean = false;
  protected accessor tooltipText_: string = '';
  accessor searchFilter: string = '';
  protected accessor filteredSites_: SiteException[] = [];

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   */
  private activeDialogAnchor_: HTMLElement|null = null;
  private browserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  constructor() {
    super();

    this.updateCategoryWarning_();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingSitePermissionChanged',
        (category: ContentSettingsTypes) =>
            this.siteWithinCategoryChanged_(category));
    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) =>
            this.siteWithinCategoryChanged_(category));
    this.addWebUiListener(
        'onIncognitoStatusChanged',
        (hasIncognito: boolean) =>
            this.onIncognitoStatusChanged_(hasIncognito));
    this.addWebUiListener(
        'osGlobalPermissionChanged', (messages: ContentSettingsTypes[]) => {
          this.setCategoryWarning_(
              !!this.category && messages.includes(this.category));
        });
    this.browserProxy.updateIncognitoStatus();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('category') ||
        changedProperties.has('categorySubtype') ||
        changedProperties.has('cookiesExceptionType')) {
      this.configureWidget_();
    }

    if (changedProperties.has('sites') ||
        changedProperties.has('searchFilter')) {
      this.filteredSites_ = this.getFilteredSites_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('systemPermissionWarningKey_')) {
      // Set imperatively rather than via a template binding, so that
      // unrelated re-renders do not replace the <a> element (and the click
      // listener attached to it below).
      this.$.systemPermissionWarningText.innerHTML =
          this.getSystemPermissionWarning_();
      this.attachSystemPermissionSettingsLinkClick_();
    }
  }

  /**
   * Update the category warning when the OS permission for this category
   * changed.
   */
  private updateCategoryWarning_() {
    this.browserProxy.getSystemDeniedPermissions().then(
        (messages: ContentSettingsTypes[]) => {
          this.setCategoryWarning_(
              !!this.category && messages.includes(this.category));
        });
  }

  /**
   * Sets the category warning when the OS permission for this category changed.
   */
  private setCategoryWarning_(categoryBlocked: boolean) {
    // The warning key is null in case the category is not one of the listed,
    // as the warning in case of an OS level block is supported only for
    // camera, microphone and location permissions.
    if (!categoryBlocked || !this.category) {
      this.systemPermissionWarningKey_ = null;
      return;
    }
    switch (this.category) {
      case ContentSettingsTypes.CAMERA:
        this.systemPermissionWarningKey_ =
            'siteSettingsContentCameraBlockedByOs';
        break;
      case ContentSettingsTypes.MIC:
        this.systemPermissionWarningKey_ = 'siteSettingsContentMicBlockedByOs';
        break;
      case ContentSettingsTypes.GEOLOCATION:
        this.systemPermissionWarningKey_ =
            'siteSettingsContentLocationBlockedByOs';
        break;
      default:
        this.systemPermissionWarningKey_ = null;
        break;
    }
  }

  /**
   * Called when a site changes permission.
   * @param category The category of the site that changed.
   */
  private siteWithinCategoryChanged_(category: ContentSettingsTypes) {
    if (category === this.category) {
      this.configureWidget_();
    }
  }

  /**
   * Called for each site list when incognito is enabled or disabled. Only
   * called on change (opening N incognito windows only fires one message).
   * Another message is sent when the *last* incognito window closes.
   */
  private onIncognitoStatusChanged_(hasIncognito: boolean) {
    this.hasIncognito_ = hasIncognito;

    // The SESSION_ONLY list won't have any incognito exceptions. (Minor
    // optimization, not required).
    if (this.categorySubtype === ContentSetting.SESSION_ONLY) {
      return;
    }

    // A change notification is not sent for each site. So we repopulate the
    // whole list when the incognito profile is created or destroyed.
    this.populateList_();
  }

  /**
   * Configures the action menu and shows the list.
   */
  private configureWidget_() {
    if (!this.category || this.categorySubtype === INVALID_CATEGORY_SUBTYPE) {
      return;
    }

    this.setUpActionMenu_();
    this.populateList_();
  }

  protected shouldHideCategory_(): boolean {
    // The Session permissions are only for cookies.
    return this.categorySubtype === ContentSetting.SESSION_ONLY &&
        this.category !== ContentSettingsTypes.COOKIES;
  }

  /** Whether there are any site exceptions added for this content setting. */
  protected hasSites_(): boolean {
    return this.sites.length > 0;
  }

  /** Whether the header warning should be shown. */
  protected shouldShowHeaderWarning_(): boolean {
    return this.hasSites_() && (this.systemPermissionWarningKey_ !== null);
  }

  /** The text of the warning. Empty if the warning is not to be shown. */
  private getSystemPermissionWarning_(): TrustedHTML {
    const sanitizeOptions: SanitizeInnerHtmlOpts = {tags: ['a'], attrs: ['id']};
    if (this.systemPermissionWarningKey_ !== null) {
      return this.i18nAdvanced(
          this.systemPermissionWarningKey_, sanitizeOptions);
    }
    return sanitizeInnerHtml('');
  }

  /** Attempts to open the system permission settings. */
  private onSystemPermissionSettingsLinkClick_(event: MouseEvent) {
    // Prevents navigation to href='#'.
    event.preventDefault();
    if (this.category) {
      this.browserProxy.openSystemPermissionSettings(this.category);
    }
  }

  /** Attached the click action to the anchor element. */
  private attachSystemPermissionSettingsLinkClick_(): void {
    const elementId = 'openSystemSettingsLink';
    const element: HTMLElement|null|undefined =
        this.shadowRoot.querySelector(`#${elementId}`);
    if (element) {
      element.addEventListener('click', (me: MouseEvent) => {
        this.onSystemPermissionSettingsLinkClick_(me);
      });
      // Set the correct aria label describing the link target.
      const settingsPageName: string|null = (() => {
        switch (this.category) {
          case ContentSettingsTypes.CAMERA:
            return 'Camera';
          case ContentSettingsTypes.MIC:
            return 'Microphone';
          case ContentSettingsTypes.GEOLOCATION:
            return 'Location';
          default:
            return null;
        }
      })();
      if (settingsPageName) {
        element.setAttribute(
            'aria-label', `System Settings: ${settingsPageName}`);
      }
    }
  }

  /**
   * Whether the Add Site button is shown in the header for the current category
   * and category subtype.
   */
  protected shouldShowAddSiteButton_(): boolean {
    return !(
        this.readOnlyList ||
        (this.category === ContentSettingsTypes.FILE_SYSTEM_WRITE &&
         this.categorySubtype === ContentSetting.ALLOW));
  }

  protected shouldShowNoSearchResults_(): boolean {
    return this.sites.length > 0 && this.filteredSites_.length === 0;
  }

  /**
   * A handler for the Add Site button.
   */
  protected onAddSiteClick_() {
    assert(!this.readOnlyList);
    assert(this.categorySubtype !== INVALID_CATEGORY_SUBTYPE);
    this.showAddSiteDialog_ = true;
  }

  protected onAddSiteDialogClose_() {
    this.showAddSiteDialog_ = false;
    focusWithoutInk(this.$.addSite);
  }

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the list.
   */
  protected async onShowTooltip_(
      e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
    await this.updateComplete;
    // cr-tooltip normally determines the target from the |for| property,
    // which is a selector. Here cr-tooltip is being reused by multiple
    // potential targets.
    this.showTooltipAtTarget(this.$.tooltip, e.detail.target);
  }

  /**
   * Populate the sites list for display.
   */
  private populateList_() {
    if (!this.category) {
      return;
    }
    this.browserProxy_.getExceptionList(this.category).then(exceptionList => {
      this.processExceptions_(exceptionList);
      this.closeActionMenu_();
    });
  }

  /**
   * Process the exception list returned from the native layer.
   */
  private processExceptions_(exceptionList: RawSiteException[]) {
    const sites = exceptionList
                      .filter(site => {
                        return site.setting !== ContentSetting.DEFAULT &&
                            site.setting === this.categorySubtype;
                      })
                      .filter(site => {
                        if (this.category !== ContentSettingsTypes.COOKIES) {
                          return true;
                        }
                        assert(this.cookiesExceptionType);
                        switch (this.cookiesExceptionType) {
                          case CookiesExceptionType.THIRD_PARTY:
                            return site.origin === SITE_EXCEPTION_WILDCARD;
                          case CookiesExceptionType.SITE_DATA:
                            // Site data exceptions include all exceptions that
                            // have `origin` set. This includes site data
                            // exceptions and exceptions with both patterns set
                            // (currently possible only via exceptions API).
                            return site.origin !== SITE_EXCEPTION_WILDCARD;
                          case CookiesExceptionType.COMBINED:
                            // For cookies exception type COMBINED, don't apply
                            // any filters and show exceptions with both pattern
                            // types.
                            return true;
                          default:
                            assertNotReachedCase(this.cookiesExceptionType);
                        }
                      })
                      .map(site => this.expandSiteException(site));

    this.sites = sites;
  }

  /**
   * Set up the values to use for the action menu.
   */
  private setUpActionMenu_() {
    this.showAllowAction_ = this.categorySubtype !== ContentSetting.ALLOW;
    this.showBlockAction_ = this.categorySubtype !== ContentSetting.BLOCK;
    this.showSessionOnlyAction_ =
        this.categorySubtype !== ContentSetting.SESSION_ONLY &&
        this.category === ContentSettingsTypes.COOKIES;
  }

  /**
   * @return Whether to show the "Session Only" menu item for the currently
   *     active site.
   */
  protected shouldShowSessionOnlyActionForSite_(): boolean {
    // It makes no sense to show "clear on exit" for exceptions that only apply
    // to incognito. It gives the impression that they might under some
    // circumstances not be cleared on exit, which isn't true.
    if (!this.actionMenuSite_ || this.actionMenuSite_.incognito) {
      return false;
    }

    return this.showSessionOnlyAction_;
  }

  private setContentSettingForActionMenuSite_(contentSetting: ContentSetting) {
    assert(this.actionMenuSite_);
    assert(this.category);
    this.browserProxy.setCategoryPermissionForPattern(
        this.actionMenuSite_.origin, this.actionMenuSite_.embeddingOrigin,
        this.category, contentSetting, this.actionMenuSite_.incognito);
  }

  protected onAllowClick_() {
    // Removing the last visible item should focus the list's header.
    const shouldMoveFocus = this.hasOneFilteredSite_();
    this.setContentSettingForActionMenuSite_(ContentSetting.ALLOW);
    this.closeActionMenu_();
    if (shouldMoveFocus) {
      this.$.listHeader.focus();
    }
  }

  protected onBlockClick_() {
    // Removing the last visible item should focus the list's header.
    const shouldMoveFocus = this.hasOneFilteredSite_();
    this.setContentSettingForActionMenuSite_(ContentSetting.BLOCK);
    this.closeActionMenu_();
    if (shouldMoveFocus) {
      this.$.listHeader.focus();
    }
  }

  protected onSessionOnlyClick_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.SESSION_ONLY);
    this.closeActionMenu_();
  }

  protected onEditClick_() {
    // Close action menu without resetting |this.actionMenuSite_| since it is
    // bound to the dialog.
    this.$.actionMenu.close();
    this.showEditExceptionDialog_ = true;
  }

  protected onEditExceptionDialogClose_() {
    this.showEditExceptionDialog_ = false;
    this.actionMenuSite_ = null;
    if (this.activeDialogAnchor_) {
      this.activeDialogAnchor_.focus();
      this.activeDialogAnchor_ = null;
    }
  }

  protected onResetClick_() {
    // Removing the last visible item should focus the list's header.
    const shouldMoveFocus = this.hasOneFilteredSite_();
    assert(this.actionMenuSite_);
    assert(this.category);
    this.browserProxy.resetCategoryPermissionForPattern(
        this.actionMenuSite_.origin, this.actionMenuSite_.embeddingOrigin,
        this.category, this.actionMenuSite_.incognito);
    this.closeActionMenu_();
    if (shouldMoveFocus) {
      this.$.listHeader.focus();
    }
  }

  protected onShowActionMenu_(
      e: CustomEvent<{anchor: HTMLElement, model: SiteException}>) {
    this.activeDialogAnchor_ = e.detail.anchor;
    this.actionMenuSite_ = e.detail.model;
    this.$.actionMenu.showAt(this.activeDialogAnchor_);
  }

  protected onSiteListEntryResetClick_() {
    // Removing the last visible item should focus the list's header.
    if (this.hasOneFilteredSite_()) {
      this.$.listHeader.focus();
    }
  }

  private closeActionMenu_() {
    this.actionMenuSite_ = null;
    this.activeDialogAnchor_ = null;
    if (this.$.actionMenu.open) {
      this.$.actionMenu.close();
    }
  }

  private getFilteredSites_(): SiteException[] {
    if (!this.searchFilter) {
      return this.sites.slice();
    }

    type SearchableProperty = 'displayName'|'origin'|'embeddingOrigin';
    const propNames: SearchableProperty[] =
        ['displayName', 'origin', 'embeddingOrigin'];
    const searchFilter = this.searchFilter.toLowerCase();
    return this.sites.filter(
        site => propNames.some(
            propName => site[propName].toLowerCase().includes(searchFilter)));
  }

  protected hasOneFilteredSite_(): boolean {
    return this.filteredSites_.length === 1;
  }

  protected getAddButtonLabel_(): string {
    if (this.categorySubtype === ContentSetting.ALLOW) {
      return this.i18n('siteDataPageAddSiteToAllowListLabel');
    } else if (this.categorySubtype === ContentSetting.BLOCK) {
      return this.i18n('siteDataPageAddSiteToBlockListLabel');
    } else {
      return '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-list': SiteListElement;
  }
}

customElements.define(SiteListElement.is, SiteListElement);
