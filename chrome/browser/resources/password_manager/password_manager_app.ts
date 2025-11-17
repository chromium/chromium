// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '/shared/settings/prefs/prefs.js';
import './checkup_section.js';
import './checkup_details_section.js';
import './password_details_section.js';
import './password_change_details.js';
import './passwords_exporter.js';
import './passwords_section.js';
import './settings_section.js';
import './shared_style.css.js';
import './side_bar.js';
import './toolbar.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {SettingsPrefsElement} from '/shared/settings/prefs/prefs.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement, listenOnce} from 'chrome://resources/js/util.js';
import type {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CheckupSectionElement} from './checkup_section.js';
import type {BackupPasswordRemovedEvent} from './credential_details/backup_password_details_card.js';
import type {PasswordRemovedEvent} from './credential_details/password_details_card.js';
import type {FocusConfig} from './focus_config.js';
import {getTemplate} from './password_manager_app.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {PasswordsSectionElement} from './passwords_section.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router} from './router.js';
import type {SettingsSectionElement} from './settings_section.js';
import type {PasswordManagerSideBarElement} from './side_bar.js';
import type {PasswordManagerToolbarElement} from './toolbar.js';

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
    drawerTemplate: DomIf,
    passwords: PasswordsSectionElement,
    prefs: SettingsPrefsElement,
    toast: CrToastElement,
    settings: SettingsSectionElement,
    sidebar: PasswordManagerSideBarElement,
    toolbar: PasswordManagerToolbarElement,
  };
}

const PasswordManagerAppElementBase = FindShortcutMixin(
    I18nMixin(CrContainerShadowMixin(RouteObserverMixin(PolymerElement))));

export class PasswordManagerAppElement extends PasswordManagerAppElementBase {
  static get is() {
    return 'password-manager-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs_: Object,

      selectedPage_: {
        type: String,
        value: Page.PASSWORDS,
      },

      narrow_: {
        type: Boolean,
        observer: 'onMaxWidthChanged_',
      },

      collapsed_: {
        type: Boolean,
        observer: 'onMaxWidthChanged_',
      },

      pageTitle_: {
        type: String,
        value: () => loadTimeData.getString('passwordManagerTitle'),
      },

      /*
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      pagesValueEnum_: {
        type: Object,
        value: Page,
      },

      toastMessage_: String,

      /**
       * Whether to show an "undo" button on the removal toast.
       */
      showUndo_: Boolean,

      /**
       * A Map specifying which element should be focused when exiting a
       * subpage. The key of the map holds a Route path, and the value holds
       * either a query selector that identifies the desired element, an element
       * or a function to be run when a neon-animation-finish event is handled.
       */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          return map;
        },
      },
    };
  }

  declare private prefs_: {[key: string]: any};
  declare private selectedPage_: Page;
  declare private narrow_: boolean;
  declare private collapsed_: boolean;
  declare private pageTitle_: string;
  declare private toastMessage_: string;
  declare private showUndo_: boolean;
  declare private focusConfig_: FocusConfig;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

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
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override ready() {
    super.ready();

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
      this.$.drawerTemplate.if = true;
    });

    this.addEventListener('cr-toolbar-menu-click', this.onMenuButtonClick_);
    this.addEventListener('close-drawer', this.closeDrawer_);
  }

  override currentRouteChanged(route: Route): void {
    this.selectedPage_ = route.page;
    setTimeout(() => {  // Async to allow page to load.
      if (route.page === Page.CHECKUP_DETAILS ||
          route.page === Page.PASSWORD_CHANGE) {
        this.enableScrollObservation(false);
        this.setForceDropShadows(true);
      } else {
        this.setForceDropShadows(false);
        this.enableScrollObservation(true);
      }
    }, 0);
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

  private onMaxWidthChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }
    // Window is greater than 1300px but less than 1605px.
    if (!this.narrow_ && this.collapsed_) {
      this.pageTitle_ = this.i18n('passwordManagerString');
    } else {
      this.pageTitle_ = this.i18n('passwordManagerTitle');
    }
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

  private showPage(currentPage: string, pageToShow: string): boolean {
    return currentPage === pageToShow;
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

  private onPasswordRemoved_(_event: PasswordRemovedEvent) {
    // TODO(crbug.com/40234318): Show different message if account store user.
    this.showUndo_ = true;
    this.toastMessage_ = this.i18n('passwordDeleted');
    this.$.toast.show();
  }

  private onBackupPasswordRemoved_(_event: BackupPasswordRemovedEvent) {
    this.showUndo_ = true;
    this.toastMessage_ = this.i18n('passwordDeleted');
    this.$.toast.show();
  }

  private onPasskeyRemoved_() {
    this.showUndo_ = false;
    this.toastMessage_ = this.i18n('passkeyDeleted');
    this.$.toast.show();
  }

  private async onPasswordsMoved_(event: PasswordsMovedEvent) {
    this.showUndo_ = false;
    this.toastMessage_ =
        await PluralStringProxyImpl.getInstance()
            .getPluralString(
                'passwordsMovedToastMessage', event.detail.numberOfPasswords)
            .then(label => label.replace('$1', event.detail.accountEmail));
    this.$.toast.show();
  }

  private onValueCopied_(event: ValueCopiedEvent) {
    this.showUndo_ = false;
    this.toastMessage_ = event.detail.toastMessage;
    this.$.toast.show();
  }

  private onUndoButtonClick_() {
    PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
    this.$.toast.hide();
  }

  private onSearchEnterClick_() {
    this.$.passwords.focusFirstResult();
  }

  private onIronSelect_(e: Event) {
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
}
declare global {
  interface HTMLElementTagNameMap {
    'password-manager-app': PasswordManagerAppElement;
  }
}

customElements.define(PasswordManagerAppElement.is, PasswordManagerAppElement);
