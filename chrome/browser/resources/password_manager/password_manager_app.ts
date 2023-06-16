// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import './checkup_section.js';
import './checkup_details_section.js';
import './password_details_section.js';
import './passwords_exporter.js';
import './passwords_section.js';
import './settings_section.js';
import './shared_style.css.js';
import './side_bar.js';
import './toolbar.js';

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {SettingsPrefsElement} from 'chrome://resources/cr_components/settings_prefs/prefs.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {getDeepActiveElement, listenOnce} from 'chrome://resources/js/util_ts.js';
import {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {DomIf, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CheckupSectionElement} from './checkup_section.js';
import {PasswordRemovedEvent} from './credential_details/password_details_card.js';
import {FocusConfig} from './focus_config.js';
import {getTemplate} from './password_manager_app.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordsSectionElement} from './passwords_section.js';
import {Page, Route, RouteObserverMixin, Router} from './router.js';
import {SettingsSectionElement} from './settings_section.js';
import {PasswordManagerSideBarElement} from './side_bar.js';
import {PasswordManagerToolbarElement} from './toolbar.js';

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

export interface PasswordManagerAppElement {
  $: {
    checkup: CheckupSectionElement,
    content: IronPagesElement,
    drawer: CrDrawerElement,
    drawerTemplate: DomIf,
    passwords: PasswordsSectionElement,
    prefs: SettingsPrefsElement,
    removalToast: CrToastElement,
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

      selectedPage_: String,

      narrow_: {
        type: Boolean,
        observer: 'onNarrowChanged_',
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

  private selectedPage_: Page;
  private narrow_: boolean;
  private toastMessage_: string;
  private showUndo_: boolean;
  private focusConfig_: FocusConfig;

  override ready() {
    super.ready();

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
      if (route.page === Page.CHECKUP_DETAILS) {
        this.enableShadowBehavior(false);
        this.showDropShadows();
      } else {
        this.enableShadowBehavior(true);
      }
    }, 0);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.$.toolbar.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.$.toolbar.searchField.isSearchFocused();
  }

  private onNarrowChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
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
    // TODO(crbug.com/1350947): Show different message if account store user.
    this.showUndo_ = true;
    this.toastMessage_ = this.i18n('passwordDeleted');
    this.$.removalToast.show();
  }

  private onPasskeyRemoved_() {
    this.showUndo_ = false;
    this.toastMessage_ = this.i18n('passkeyDeleted');
    this.$.removalToast.show();
  }

  private onUndoButtonClick_() {
    PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
    this.$.removalToast.hide();
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
