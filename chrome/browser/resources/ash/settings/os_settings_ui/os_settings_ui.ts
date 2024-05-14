// Copyright 2019 The Chromium Authors
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
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../os_settings_menu/os_settings_menu.js';
import '../os_settings_main/os_settings_main.js';
import '../toolbar/toolbar.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {SettingsPrefsElement} from '/shared/settings/prefs/prefs.js';
import {CrContainerShadowMixin} from 'chrome://resources/ash/common/cr_elements/cr_container_shadow_mixin.js';
import {CrDrawerElement} from 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import {FindShortcutMixin} from 'chrome://resources/ash/common/cr_elements/find_shortcut_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {Debouncer, DomIf, microTask, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {setGlobalScrollTarget} from '../common/global_scroll_target_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import type {UserActionSettingPrefChangeEvent} from '../common/types.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSettingChange, recordSettingChangeForUnmappedPref} from '../metrics_recorder.js';
import {convertPrefToSettingMetric} from '../metrics_utils.js';
import {createPageAvailability, OsPageAvailability} from '../os_page_availability.js';
import {Route, Router} from '../router.js';
import {SettingsToolbarElement} from '../toolbar/toolbar.js';

import {OsSettingsHatsBrowserProxy, OsSettingsHatsBrowserProxyImpl} from './os_settings_hats_browser_proxy.js';
import {getTemplate} from './os_settings_ui.html.js';

declare global {
  interface Window {
    settings: any;
  }
}

declare global {
  interface HTMLElementEventMap {
    'refresh-pref': CustomEvent<string>;
    'scroll-to-bottom': CustomEvent<{bottom: number, callback: () => void}>;
    'scroll-to-top': CustomEvent<{top: number, callback: () => void}>;
    'user-action-setting-change':
        CustomEvent<{prefKey: string, prefValue: any}>;
    'user-action-setting-pref-change': UserActionSettingPrefChangeEvent;
  }
}

/** Global defined when the main Settings script runs. */
let defaultResourceLoaded = true;  // eslint-disable-line prefer-const

assert(
    !window.settings || !defaultResourceLoaded,
    'os_settings_ui.js was executed twice. You probably have an invalid import.');

export interface OsSettingsUiElement {
  $: {
    container: HTMLDivElement,
    prefs: SettingsPrefsElement,
  };
}

const OsSettingsUiElementBase =
    // RouteObserverMixin calls currentRouteChanged() in
    // connectedCallback(), so ensure other mixins/behaviors run their
    // connectedCallback() first.
    RouteObserverMixin(
        FindShortcutMixin(CrContainerShadowMixin(PolymerElement)));

export class OsSettingsUiElement extends OsSettingsUiElementBase {
  static get is() {
    return 'os-settings-ui';
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

      advancedOpenedInMain_: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onAdvancedOpenedInMainChanged_',
      },

      advancedOpenedInMenu_: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onAdvancedOpenedInMenuChanged_',
      },

      toolbarSpinnerActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether settings is in the narrow state (side nav hidden). Controlled
       * by a binding in the `settings-toolbar` element.
       */
      isNarrow: {
        type: Boolean,
        value: false,
        readonly: true,
        notify: true,
        observer: 'onNarrowChanged_',
      },

      /**
       * Used to determine which pages and menu items are available (not to be
       * confused with page visibility) to the user.
       * See os_page_availability.ts for more details.
       */
      pageAvailability_: {
        type: Object,
        value: () => {
          return createPageAvailability();
        },
      },

      showToolbar_: Boolean,

      showNavMenu_: Boolean,

      /**
       * The threshold at which the toolbar will change from normal to narrow
       * mode, in px.
       */
      narrowThreshold_: {
        type: Number,
        value: 980,
      },
    };
  }

  prefs: Object;
  isNarrow: boolean;
  private advancedOpenedInMain_: boolean;
  private advancedOpenedInMenu_: boolean;
  private toolbarSpinnerActive_: boolean;
  private pageAvailability_: OsPageAvailability;
  private showToolbar_: boolean;
  private showNavMenu_: boolean;
  private narrowThreshold_: number;
  private activeRoute_: Route|null;
  private scrollEndDebouncer_: Debouncer|null;
  private osSettingsHatsBrowserProxy_: OsSettingsHatsBrowserProxy;
  private boundTriggerSettingsHats_: () => void;
  private readonly isRevampWayfindingEnabled_: boolean =
      isRevampWayfindingEnabled();

  constructor() {
    super();

    /**
     * The route of the selected element in os-settings-menu. Stored here to
     * defer navigation until drawer animation completes.
     */
    this.activeRoute_ = null;

    this.scrollEndDebouncer_ = null;

    Router.getInstance().initializeRouteFromUrl();

    this.osSettingsHatsBrowserProxy_ =
        OsSettingsHatsBrowserProxyImpl.getInstance();

    this.boundTriggerSettingsHats_ = this.triggerSettingsHats_.bind(this);
  }

  override ready(): void {
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
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      controlledSettingParent:
          loadTimeData.getString('controlledSettingParent'),
      controlledSettingChildRestriction:
          loadTimeData.getString('controlledSettingChildRestriction'),
    };

    this.showNavMenu_ = !loadTimeData.getBoolean('isKioskModeActive');
    this.showToolbar_ = !loadTimeData.getBoolean('isKioskModeActive');

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });

    this.addEventListener('refresh-pref', this.onRefreshPref_);

    this.addEventListener('user-action-setting-pref-change', this.syncPrefChange_.bind(this));

    this.addEventListener('user-action-setting-change', this.recordChangedSetting_.bind(this));

    this.addEventListener(
        'search-changed',
        () => {
          this.osSettingsHatsBrowserProxy_.settingsUsedSearch();
        },
        /*AddEventListenerOptions=*/ {once: true});

    this.listenForDrawerOpening_();

    // By default, the shadow should show when the container is scrolled down.
    this.enableShadowBehavior(true);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    setTimeout(() => {
      this.recordTimeUntilInteractive_();
    });

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    setGlobalScrollTarget(this.$.container);

    const scrollToTop = (top: number): Promise<void> => new Promise(resolve => {
      if (this.$.container.scrollTop === top) {
        resolve();
        return;
      }

      this.$.container.scrollTo({top: top, behavior: 'auto'});
      const onScroll = (): void => {
        this.scrollEndDebouncer_ = Debouncer.debounce(
            this.scrollEndDebouncer_, timeOut.after(75), () => {
              this.$.container.removeEventListener('scroll', onScroll);
              resolve();
            });
      };
      this.$.container.addEventListener('scroll', onScroll);
    });
    this.addEventListener(
        'scroll-to-top',
        (e: CustomEvent<{top: number, callback: () => void}>) => {
          scrollToTop(e.detail.top).then(e.detail.callback);
        });
    this.addEventListener(
        'scroll-to-bottom',
        (e: CustomEvent<{bottom: number, callback: () => void}>) => {
          scrollToTop(e.detail.bottom - this.$.container.clientHeight)
              .then(e.detail.callback);
        });

    // Window event listeners will not fire when settings first starts.
    // Blur events before the first focus event do not matter.
    if (document.hasFocus()) {
      recordPageFocus();
    }

    window.addEventListener('focus', recordPageFocus);
    window.addEventListener('blur', recordPageBlur);

    window.addEventListener('blur', this.boundTriggerSettingsHats_);

    // Clicks need to be captured because unlike focus/blur to the settings
    // window, a click's propagation can be stopped by child elements.
    window.addEventListener('click', recordClick, /*capture=*/ true);

    if (this.isRevampWayfindingEnabled_) {
      // Add class which activates styles for the wayfinding update
      document.body.classList.add('revamp-wayfinding-enabled');
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    window.removeEventListener('focus', recordPageFocus);
    window.removeEventListener('blur', recordPageBlur);
    window.removeEventListener('blur', this.boundTriggerSettingsHats_);
    window.removeEventListener('click', recordClick);
    Router.getInstance().resetRouteForTesting();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    if (oldRoute && newRoute !== oldRoute) {
      // Search triggers route changes and currentRouteChanged() is called
      // in attached() state which is extraneous for this metric.
      recordNavigation();
    }

    // TODO(b/302374851) Under the revamp, the shadow behavior is consistent
    // across all types of pages and subpages. When the revamp is cleaned up,
    // remove this obsolete logic.
    if (!this.isRevampWayfindingEnabled_) {
      if (newRoute.isSubpage()) {
        // Sub-pages always show the top-container shadow.
        this.enableShadowBehavior(false);
        this.showDropShadows();
      } else {
        // All other pages including the root page should show shadow depending
        // on scroll position.
        this.enableShadowBehavior(true);
      }
    }
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen || !this.showToolbar_) {
      return false;
    }
    const toolbar = this.getToolbar_();
    toolbar.getSearchField().showAndFocus();
    toolbar.getSearchField().getSearchInput().select();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    if (!this.showToolbar_) {
      return false;
    }

    return this.getToolbar_().getSearchField().isSearchFocused();
  }

  /**
   * Listen for the drawer opening event and lazily create the drawer the first
   * time it is opened or swiped into view.
   */
  private listenForDrawerOpening_(): void {
    // If navigation menu is not shown, do not listen for the drawer opening
    if (!this.showNavMenu_) {
      return;
    }

    microTask.run(() => {
      const drawer = this.getDrawer_();
      listenOnce(drawer, 'cr-drawer-opening', () => {
        const drawerTemplate = castExists(
            this.shadowRoot!.querySelector<DomIf>('#drawerTemplate'));
        drawerTemplate.if = true;
      });

      window.addEventListener('popstate', () => {
        drawer.cancel();
      });
    });
  }

  private getDrawer_(): CrDrawerElement {
    return castExists(this.shadowRoot!.querySelector('cr-drawer'));
  }

  private getToolbar_(): SettingsToolbarElement {
    return castExists(this.shadowRoot!.querySelector('settings-toolbar'));
  }

  private onRefreshPref_(e: CustomEvent<string>): void {
    this.$.prefs.refresh(e.detail);
  }

  /**
   * Callback for the `user-action-setting-change` event which is emitted by
   * the `settings-prefs` singleton after a pref-based setting is updated via
   * some user action. Records the changed setting to relevant metrics.
   */
  private recordChangedSetting_(e: CustomEvent<{prefKey: string, prefValue: any}>):
      void {
    const {prefKey, prefValue} = e.detail;
    const settingMetric = convertPrefToSettingMetric(prefKey, prefValue);

    // New metrics for this setting pref have not yet been implemented.
    if (!settingMetric) {
      recordSettingChangeForUnmappedPref();
      return;
    }

    recordSettingChange(settingMetric.setting, settingMetric.value);
  }

  /**
   * Callback for the `user-action-setting-pref-change` event which is emitted
   * by settings pref control components when the prefs state should be synced
   * after some user action (e.g. a toggle was turned on). Updates the prefs
   * state and syncs it with the `settings-prefs` singleton, which applies the
   * update at the OS level.
   */
  private syncPrefChange_(event: UserActionSettingPrefChangeEvent): void {
    const {prefKey, value} = event.detail;
    this.set(`prefs.${prefKey}.value`, value);
  }

  /**
   * Called when a menu item is selected.
   */
  private onMenuItemSelected_(e: CustomEvent<{selected: string}>): void {
    assert(this.showNavMenu_);
    const path = e.detail.selected;
    const route = Router.getInstance().getRouteForPath(path);
    assert(route, `os-settings-menu-item with invalid route: ${path}`);
    this.activeRoute_ = route;

    if (this.isNarrow) {
      // If the onIronActivate event came from the drawer, close the drawer
      // and wait for the menu to close before navigating to |activeRoute_|.
      this.getDrawer_().close();
      return;
    }
    this.navigateToActiveRoute_();
  }

  private onMenuButtonClick_(): void {
    if (!this.showNavMenu_) {
      return;
    }
    this.getDrawer_().toggle();
  }

  /**
   * Navigates to |activeRoute_| if set. Used to delay navigation until after
   * animations complete to ensure focus ends up in the right place.
   */
  private navigateToActiveRoute_(): void {
    if (this.activeRoute_) {
      Router.getInstance().navigateTo(
          this.activeRoute_, /* dynamicParams */ undefined,
          /* removeSearch */ true);
      this.activeRoute_ = null;
    }
  }

  /**
   * When this is called, The drawer animation is finished, and the dialog no
   * longer has focus. The selected section will gain focus if one was
   * selected. Otherwise, the drawer was closed due being canceled, and the
   * main settings container is given focus. That way the arrow keys can be
   * used to scroll the container, and pressing tab focuses a component in
   * settings.
   */
  private onMenuClose_(): void {
    if (!this.getDrawer_().wasCanceled()) {
      // If a navigation happened, MainPageMixin#currentRouteChanged
      // handles focusing the corresponding section when we call
      // settings.NavigateTo().
      this.navigateToActiveRoute_();
      return;
    }

    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  }

  private onAdvancedOpenedInMainChanged_(): void {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  }

  private onAdvancedOpenedInMenuChanged_(): void {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  }

  private onNarrowChanged_(): void {
    if (this.showNavMenu_) {
      const drawer = this.getDrawer_();
      if (drawer.open && !this.isNarrow) {
        drawer.close();
      }
    }
  }

  /**
   * Handles a tap on the drawer's icon.
   */
  private onDrawerIconClick_(): void {
    this.getDrawer_().cancel();
  }

  private recordTimeUntilInteractive_(): void {
    const METRIC_NAME = 'ChromeOS.Settings.TimeUntilInteractive';
    const timeMs = Math.round(window.performance.now());
    chrome.metricsPrivate.recordTime(METRIC_NAME, timeMs);
  }

  private triggerSettingsHats_(): void {
    this.osSettingsHatsBrowserProxy_.sendSettingsHats();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-ui': OsSettingsUiElement;
  }
}

customElements.define(OsSettingsUiElement.is, OsSettingsUiElement);
