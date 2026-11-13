// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './checkup_section.js';
import './checkup_details_section.js';
import './password_details_section.js';
import './password_change_details.js';
import './passwords_exporter.js';
import './passwords_section.js';
import './settings_section.js';
import './side_bar.js';
import './toolbar.js';
import './dialogs/trusted_vault_error_dialog.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement, listenOnce} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CheckupSectionElement} from './checkup_section.js';
import type {BackupPasswordRemovedEvent} from './credential_details/backup_password_details_card.js';
import type {PasswordRemovedEvent} from './credential_details/password_details_card.js';
import type {FocusConfig} from './focus_config.js';
import {getCss} from './password_manager_app.css.js';
import {getHtml} from './password_manager_app.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {PasswordsSectionElement} from './passwords_section.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router} from './router.js';
import type {SettingsSectionElement} from './settings_section.js';
import type {PasswordManagerSideBarElement} from './side_bar.js';
import type {PasswordManagerToolbarElement} from './toolbar.js';
import type {ShowTrustedVaultErrorDialogEvent} from './user_utils_mixin.js';
import {TrustedVaultUnlockDialogSource, UserUtilMixin} from './user_utils_mixin.js';

/**
 * Checks if an HTML element is an editable. An editable is either a text
 * input or a text area.
 */
function isEditable(element: Element): boolean {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(
            (element as HTMLInputElement).type)));
}

export type PasswordsMovedEvent =
    CustomEvent<{accountEmail: string, numberOfPasswords: number}>;

export type ValueCopiedEvent = CustomEvent<{toastMessage: string}>;

export interface PasswordManagerAppElement {
  $: {
    checkup: CheckupSectionElement,
    content: CrPageSelectorElement,
    drawer: CrDrawerElement,
    drawerTemplate: CrLazyRenderLitElement<PasswordManagerSideBarElement>,
    passwords: PasswordsSectionElement,
    toast: CrToastElement,
    scrollableShadow: HTMLElement,
    settings: SettingsSectionElement,
    sidebar: PasswordManagerSideBarElement,
    toolbar: PasswordManagerToolbarElement,
  };
}

const PasswordManagerAppElementBase = UserUtilMixin(
    FindShortcutMixinLit(I18nMixinLit(RouteObserverMixin(CrLitElement))));

export class PasswordManagerAppElement extends PasswordManagerAppElementBase {
  static get is() {
    return 'password-manager-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedPage_: {type: String},
      narrow_: {type: Boolean},
      collapsed_: {type: Boolean},
      showTrustedVaultErrorDialog_: {type: Boolean},

      /** What caused the trusted vault error dialog to be shown. */
      trustedVaultErrorDialogSource_: {type: String},

      toastMessage_: {type: String},

      /**
       * Whether to show an "undo" button on the removal toast.
       */
      showUndo_: {type: Boolean},

      /**
       * A Map specifying which element should be focused when exiting a
       * subpage. The key of the map holds a Route path, and the value holds
       * either a query selector that identifies the desired element, an element
       * or a function to be run when a neon-animation-finish event is handled.
       */
      focusConfig_: {type: Object},
    };
  }

  protected accessor selectedPage_: Page = Page.PASSWORDS;
  protected accessor narrow_: boolean = false;
  private accessor collapsed_: boolean = false;
  protected accessor showTrustedVaultErrorDialog_: boolean = false;
  protected accessor trustedVaultErrorDialogSource_:
      TrustedVaultUnlockDialogSource =
          TrustedVaultUnlockDialogSource.AUTO_ON_LOAD;
  protected accessor toastMessage_: string = '';
  protected accessor showUndo_: boolean = false;
  protected accessor focusConfig_: FocusConfig = new Map();

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const enableWebuiRefresh2026 =
        loadTimeData.getString('webuiRefresh2026') !== '';
    if (enableWebuiRefresh2026) {
      this.addThemedColors_();
      ColorChangeUpdater.forDocument().start();
    }

    const narrowQuery = window.matchMedia('(max-width: 1300px)');
    this.narrow_ = narrowQuery.matches;
    this.eventTracker_.add(
        narrowQuery, 'change',
        (e: MediaQueryListEvent) => this.narrow_ = e.matches);

    const collapsedQuery = window.matchMedia('(max-width: 1605px)');
    this.collapsed_ = collapsedQuery.matches;
    this.eventTracker_.add(
        collapsedQuery, 'change',
        (e: MediaQueryListEvent) => this.collapsed_ = e.matches);

    this.eventTracker_.add(
        this, 'show-trusted-vault-error-dialog',
        (e: ShowTrustedVaultErrorDialogEvent) =>
            this.openTrustedVaultErrorDialog_(e.detail.source));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('actionableError')) {
      if (this.isTrustedVaultKeyNeeded()) {
        this.openTrustedVaultErrorDialog_(
            TrustedVaultUnlockDialogSource.AUTO_ON_LOAD);
      } else {
        this.showTrustedVaultErrorDialog_ = false;
      }
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    window.CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingExtensionWithoutName:
          loadTimeData.getString('controlledSettingExtensionWithoutName'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
      controlledSettingChildRestriction:
          loadTimeData.getString('controlledSettingChildRestriction'),
      controlledSettingParent:
          loadTimeData.getString('controlledSettingParent'),

      // <if expr="is_chromeos">
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      // </if>
    };

    document.addEventListener('keydown', e => {
      // <if expr="is_macosx">
      if (e.metaKey && e.key === 'z') {
        this.onUndoKeyBinding_(e);
      }
      // </if>
      // <if expr="not is_macosx">
      if (e.ctrlKey && e.key === 'z') {
        this.onUndoKeyBinding_(e);
      }
      // </if>
    });

    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.get();
    });

    this.addEventListener('cr-toolbar-menu-click', this.onMenuButtonClick_);
    this.addEventListener('close-drawer', this.closeDrawer_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('narrow_')) {
      if (this.$.drawer.open && !this.narrow_) {
        this.$.drawer.close();
      }
    }
  }

  override currentRouteChanged(route: Route): void {
    this.selectedPage_ = route.page;
    this.$.scrollableShadow.classList.toggle(
        'force-on',
        route.page === Page.CHECKUP_DETAILS ||
            route.page === Page.PASSWORD_CHANGE);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    // Redirect to Password Manager search on Passwords page.
    if (Router.getInstance().currentRoute.page === Page.PASSWORDS) {
      this.$.toolbar.searchField.showAndFocus();
      return true;
    }
    return false;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.$.toolbar.searchField.isSearchFocused();
  }

  private onMenuButtonClick_() {
    this.$.drawer.toggle();
  }

  private closeDrawer_() {
    if (this.$.drawer && this.$.drawer.open) {
      this.$.drawer.close();
    }
  }

  setNarrowForTesting(state: boolean) {
    this.narrow_ = state;
  }

  protected getPageTitle_(): string {
    // Window is greater than 1300px but less than 1605px.
    if (!this.narrow_ && this.collapsed_) {
      return this.i18n('passwordManagerString');
    }
    return this.i18n('passwordManagerTitle');
  }

  /**
   * Handle the shortcut to undo a removal of passwords/exceptions. This must
   * be handled here and not at the PasswordDetailsCard level because that
   * component does not know about exception deletions.
   */
  private onUndoKeyBinding_(event: Event) {
    const activeElement = getDeepActiveElement();
    // If the focused element is editable (e.g. search box) the undo event
    // should be handled there and not here.
    if (!activeElement || !isEditable(activeElement)) {
      this.onUndoButtonClick_();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  }

  protected onPasswordRemoved_(_event: PasswordRemovedEvent) {
    // TODO(crbug.com/40234318): Show different message if account store user.
    this.showUndo_ = true;
    this.toastMessage_ = this.i18n('passwordDeleted');
    this.$.toast.show();
  }

  protected onBackupPasswordRemoved_(_event: BackupPasswordRemovedEvent) {
    this.showUndo_ = true;
    this.toastMessage_ = this.i18n('passwordDeleted');
    this.$.toast.show();
  }

  protected onPasskeyRemoved_() {
    this.showUndo_ = false;
    this.toastMessage_ = this.i18n('passkeyDeleted');
    this.$.toast.show();
  }

  protected async onPasswordsMoved_(event: PasswordsMovedEvent) {
    this.showUndo_ = false;
    this.toastMessage_ =
        await PluralStringProxyImpl.getInstance()
            .getPluralString(
                'passwordsMovedToastMessage', event.detail.numberOfPasswords)
            .then(label => label.replace('$1', event.detail.accountEmail));
    this.$.toast.show();
  }

  protected onValueCopied_(event: ValueCopiedEvent) {
    this.showUndo_ = false;
    this.toastMessage_ = event.detail.toastMessage;
    this.$.toast.show();
  }

  protected onUndoButtonClick_() {
    PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
    this.$.toast.hide();
  }

  protected onSearchEnterClick_() {
    this.$.passwords.focusFirstResult();
  }

  protected onIronSelect_(e: Event) {
    // Ignore bubbling 'iron-select' events not originating from
    // |content| itself.
    if (e.target !== this.$.content) {
      return;
    }

    if (!this.focusConfig_ || Router.getInstance().previousRoute === null) {
      return;
    }

    const pathConfig =
        this.focusConfig_.get(Router.getInstance().previousRoute!.page);
    if (pathConfig) {
      let handler;
      if (typeof pathConfig === 'function') {
        handler = pathConfig;
      } else {
        handler = () => {
          focusWithoutInk(pathConfig as HTMLElement);
        };
      }
      handler();
    }
  }

  // TODO(crub.com/509908129): Add static stylesheet in password_manager.html
  private addThemedColors_() {
    assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=ui,chrome';
    document.body.appendChild(link);
  }

  private openTrustedVaultErrorDialog_(source: TrustedVaultUnlockDialogSource) {
    if (this.showTrustedVaultErrorDialog_) {
      // Keep the source of the dialog which is already shown.
      return;
    }
    this.trustedVaultErrorDialogSource_ = source;
    this.showTrustedVaultErrorDialog_ = true;
  }

  protected onTrustedVaultErrorDialogClose_() {
    this.showTrustedVaultErrorDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-app': PasswordManagerAppElement;
  }
}

customElements.define(PasswordManagerAppElement.is, PasswordManagerAppElement);
