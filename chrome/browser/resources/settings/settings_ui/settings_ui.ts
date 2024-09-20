// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../icons.html.js';
import '../settings_main/settings_main.js';
import '../settings_menu/settings_menu.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import type {SettingsPrefsElement} from '/shared/settings/prefs/prefs.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import type {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {resetGlobalScrollTargetForTesting, setGlobalScrollTarget} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import type {PageVisibility} from '../page_visibility.js';
import {pageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {SettingsMainElement} from '../settings_main/settings_main.js';
import type {SettingsMenuElement} from '../settings_menu/settings_menu.js';

import {getTemplate} from './settings_ui.html.js';

declare global {
  interface HTMLElementEventMap {
    'refresh-pref': CustomEvent<string>;
  }
}

export interface SettingsUiElement {
  $: {
    container: HTMLElement,
    drawer: CrDrawerElement,
    drawerTemplate: DomIf,
    leftMenu: SettingsMenuElement,
    main: SettingsMainElement,
    toolbar: CrToolbarElement,
    prefs: SettingsPrefsElement,
  };
}

const SettingsUiElementBase = RouteObserverMixin(
    CrContainerShadowMixin(FindShortcutMixin(PolymerElement)));

export class SettingsUiElement extends SettingsUiElementBase {
  static get is() {
    return 'settings-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: Object,

      toolbarSpinnerActive_: {
        type: Boolean,
        value: false,
      },

      narrow_: {
        type: Boolean,
        observer: 'onNarrowChanged_',
      },

      pageVisibility_: {type: Object, value: pageVisibility},

      lastSearchQuery_: {
        type: String,
        value: '',
      },
    };
  }

  private toolbarSpinnerActive_: boolean;
  private narrow_: boolean;
  private pageVisibility_: PageVisibility;
  private lastSearchQuery_: string;

  constructor() {
    super();

    Router.getInstance().initializeRouteFromUrl();
  }

  override ready() {
    super.ready();

    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.if = true;
    });

    window.addEventListener('popstate', () => {
      this.$.drawer.cancel();
    });

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

      // <if expr="chromeos_ash">
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      // </if>
    };

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });

    this.addEventListener('refresh-pref', this.onRefreshPref_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    // https://github.com/microsoft/TypeScript/issues/13569
    (document as any).fonts.load('bold 12px Roboto');
    setGlobalScrollTarget(this.$.container);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    Router.getInstance().resetRouteForTesting();
    resetGlobalScrollTargetForTesting();
  }

  override currentRouteChanged(route: Route) {
    if (route === routes.PRIVACY_GUIDE) {
      // Privacy guide has a multi-card layout, which only needs shadows to
      // show when there is more content to scroll.
      this.setForceDropShadows(false);
      this.enableScrollObservation(true);
    } else if (route.depth <= 1) {
      // Main page uses scroll position to determine whether a shadow should
      // be shown.
      this.setForceDropShadows(false);
      this.enableScrollObservation(true);
    } else if (!route.isNavigableDialog) {
      // Sub-pages always show the top shadow, regardless of scroll position.
      this.enableScrollObservation(false);
      this.setForceDropShadows(true);
    }

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search') || '';
    if (urlSearchQuery === this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const toolbar =
        this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!;
    const searchField = toolbar.getSearchField();

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery !== searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean) {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!
        .getSearchField()
        .showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus() {
    return this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!
        .getSearchField()
        .isSearchFocused();
  }

  private onRefreshPref_(e: CustomEvent<string>) {
    return this.$.prefs.refresh(e.detail);
  }

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   */
  private onSearchChanged_(e: CustomEvent<string>) {
    const query = e.detail;
    Router.getInstance().navigateTo(
        routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  }

  /**
   * Called when a section is selected.
   */
  private onIronActivate_() {
    this.$.drawer.close();
  }

  private onMenuButtonClick_() {
    this.$.drawer.toggle();
  }

  /**
   * When this is called, The drawer animation is finished, and the dialog no
   * longer has focus. The selected section will gain focus if one was
   * selected. Otherwise, the drawer was closed due being canceled, and the
   * main settings container is given focus. That way the arrow keys can be
   * used to scroll the container, and pressing tab focuses a component in
   * settings.
   */
  private onMenuClose_() {
    if (!this.$.drawer.wasCanceled()) {
      // If a navigation happened, MainPageMixin#currentRouteChanged
      // handles focusing the corresponding section.
      return;
    }

    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  }

  private onNarrowChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }

    const focusedElement = this.shadowRoot!.activeElement;
    if (this.narrow_ && focusedElement === this.$.leftMenu) {
      // If changed from non-narrow to narrow and the focus was on the left
      // menu, move focus to the button that opens the drawer menu.
      this.$.toolbar.focusMenuButton();
    } else if (!this.narrow_ && this.$.toolbar.isMenuFocused()) {
      // If changed from narrow to non-narrow and the focus was on the button
      // that opens the drawer menu, move focus to the left menu.
      this.$.leftMenu.focusFirstItem();
    } else if (
        !this.narrow_ &&
        focusedElement === this.shadowRoot!.querySelector('#drawerMenu')) {
      // If changed from narrow to non-narrow and the focus was in the drawer
      // menu, wait for the drawer to close and then move focus on the left
      // menu. The drawer has a dialog element in it so moving focus to an
      // element outside the dialog while it is open will not work.
      const boundCloseListener = () => {
        this.$.leftMenu.focusFirstItem();
        this.$.drawer.removeEventListener('close', boundCloseListener);
      };
      this.$.drawer.addEventListener('close', boundCloseListener);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ui': SettingsUiElement;
  }
}

customElements.define(SettingsUiElement.is, SettingsUiElement);
