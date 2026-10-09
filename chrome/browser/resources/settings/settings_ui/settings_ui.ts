// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui></settings-ui>
 */
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../settings_main/settings_main.js';
import '../settings_menu/settings_menu.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ActiveTimer} from '../active_timer.js';
import {resetGlobalScrollTargetForTesting, setGlobalScrollTarget} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit, Router} from '../router.js';
import {SearchMetricsRecorder} from '../search_metrics_recorder.js';
import type {SearchFinishedDetail, SettingsMainElement} from '../settings_main/settings_main.js';
import type {SettingsMenuElement} from '../settings_menu/settings_menu.js';

import {getCss} from './settings_ui.css.js';
import {getHtml} from './settings_ui.html.js';

export interface SettingsUiElement {
  $: {
    container: HTMLElement,
    drawer: CrDrawerElement,
    drawerMenu: CrLazyRenderLitElement<SettingsMenuElement>,
    left: HTMLElement,
    leftMenu: SettingsMenuElement,
    main: SettingsMainElement,
    scrollableShadow: HTMLElement,
    toolbar: CrToolbarElement,
  };
}

export const MAX_QUERY_LENGTH = 1000;

const SettingsUiElementBase =
    RouteObserverMixinLit(FindShortcutMixinLit(CrLitElement));

export class SettingsUiElement extends SettingsUiElementBase {
  static get is() {
    return 'settings-ui';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      toolbarSpinnerActive_: {type: Boolean},
      narrow_: {type: Boolean},
      lastSearchQuery_: {type: String},
      isSettingsRefresh2026_: {type: Boolean},
    };
  }

  protected accessor toolbarSpinnerActive_: boolean = false;
  protected accessor narrow_: boolean = false;
  private accessor lastSearchQuery_: string = '';
  private accessor isSettingsRefresh2026_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';

  private activeTimer_: ActiveTimer|null = null;

  private searchMetricsRecorder_: SearchMetricsRecorder =
      new SearchMetricsRecorder();

  constructor() {
    super();

    Router.getInstance().initializeRouteFromUrl();
  }

  override connectedCallback() {
    super.connectedCallback();

    const enableThemedColors =
        loadTimeData.getString('webuiRefresh2026') !== '' ||
        loadTimeData.getString('settingsRefresh2026') !== '';
    if (enableThemedColors) {
      this.addThemedColors_();
      ColorChangeUpdater.forDocument().start();
    }
    document.documentElement.classList.remove('loading');

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    setGlobalScrollTarget(this.$.container);

    this.activeTimer_ = new ActiveTimer((duration) => {
      chrome.metricsPrivate.recordLongTime(
          'WebUI.Settings.ActiveDuration', duration);
    });
    this.activeTimer_.start();

    this.searchMetricsRecorder_.start();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    Router.getInstance().resetRouteForTesting();
    resetGlobalScrollTargetForTesting();

    if (this.activeTimer_) {
      this.activeTimer_.stop();
      this.activeTimer_ = null;
    }

    this.searchMetricsRecorder_.stop();
  }

  override firstUpdated() {
    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerMenu.get();
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

      // <if expr="is_chromeos">
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      // </if>
    };
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('narrow_')) {
      this.onNarrowChanged_();
    }
  }

  override currentRouteChanged(route: Route) {
    this.$.scrollableShadow.classList.toggle(
        'force-on', route === routes.PRIVACY_GUIDE || route.depth > 1);

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search') || '';
    if (urlSearchQuery === this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const searchField = this.$.toolbar.getSearchField();

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
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.$.toolbar.getSearchField().showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.$.toolbar.getSearchField().isSearchFocused();
  }

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   */
  protected onSearchChanged_(e: CustomEvent<string>) {
    let query = e.detail;
    if (query.length > MAX_QUERY_LENGTH) {
      query = query.substring(0, MAX_QUERY_LENGTH);
    }
    Router.getInstance().navigateTo(
        routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  }

  /**
   * Handles the 'search-finished' event fired from settings-main.
   */
  protected onSearchFinished_(e: CustomEvent<SearchFinishedDetail>) {
    this.searchMetricsRecorder_.onSearchFinished(
        e.detail.query, e.detail.matchCount);
  }

  /**
   * Handles the 'search-result-interaction' event fired from settings-main.
   */
  protected onSearchResultInteraction_() {
    this.searchMetricsRecorder_.onSearchResultInteraction();
  }

  /**
   * Called when a section is selected.
   */
  protected onIronActivate_() {
    this.$.drawer.close();
  }

  protected onCrToolbarMenuClick_() {
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
  protected onMenuClose_() {
    if (!this.$.drawer.wasCanceled()) {
      // If a navigation happened, SettingsMain handles focusing the
      // corresponding section.
      return;
    }

    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  }

  protected getLeftMenuHidden_(): boolean {
    return this.narrow_ && !this.isSettingsRefresh2026_;
  }

  protected getLeftMenuCollapsed_(): boolean {
    return this.narrow_ && this.isSettingsRefresh2026_;
  }

  protected onToolbarNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected onToolbarSpinnerActiveChanged_(e: CustomEvent<{value: boolean}>) {
    this.toolbarSpinnerActive_ = e.detail.value;
  }

  private onNarrowChanged_() {
    // In SettingsRefresh2026, the left menu collapses instead of hiding and
    // the drawer menu is not used, so focus does not need to move to the
    // toolbar drawer button.
    if (this.isSettingsRefresh2026_) {
      return;
    }

    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }

    const focusedElement = this.shadowRoot.activeElement;
    if (this.narrow_ && focusedElement === this.$.leftMenu) {
      // If changed from non-narrow to narrow and the focus was on the left
      // menu, move focus to the button that opens the drawer menu.
      this.$.toolbar.focusMenuButton();
    } else if (!this.narrow_ && this.$.toolbar.isMenuFocused()) {
      // If changed from narrow to non-narrow and the focus was on the button
      // that opens the drawer menu, move focus to the left menu.
      this.$.leftMenu.focusFirstItem();
    } else if (
        !this.narrow_ && focusedElement &&
        focusedElement === this.$.drawerMenu.getIfExists()) {
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

  private addThemedColors_() {
    assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=ui,chrome';
    document.body.appendChild(link);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ui': SettingsUiElement;
  }
}

customElements.define(SettingsUiElement.is, SettingsUiElement);
