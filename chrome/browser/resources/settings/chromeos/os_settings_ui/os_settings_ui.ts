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
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../os_settings_menu/os_settings_menu.js';
import '../os_settings_main/os_settings_main.js';
import '../os_toolbar/os_toolbar.js';
import '../../settings_shared.css.js';
import '../../prefs/prefs.js';
import '../../settings_vars.css.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {Debouncer, DomIf, microTask, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {SettingsPrefsElement} from '../../prefs/prefs.js';
import {castExists} from '../assert_extras.js';
import {setGlobalScrollTarget} from '../global_scroll_target_behavior.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSettingChange} from '../metrics_recorder.js';
import {convertPrefToSettingMetric} from '../metrics_utils.js';
import {OSPageVisibility, osPageVisibility} from '../os_page_visibility.js';
import {OsToolbarElement} from '../os_toolbar/os_toolbar.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

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
  }
}

/** Global defined when the main Settings script runs. */
let defaultResourceLoaded = true;  // eslint-disable-line prefer-const

assert(
    !window.settings || !defaultResourceLoaded,
    'os_settings_ui.js was executed twice. You probably have an invalid import.');

interface OsSettingsUiElement {
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

class OsSettingsUiElement extends OsSettingsUiElementBase {
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
       * by a binding in the os-toolbar element.
       */
      isNarrow: {
        type: Boolean,
        value: false,
        readonly: true,
        notify: true,
        observer: 'onNarrowChanged_',
      },

      pageVisibility_: {type: Object, value: osPageVisibility},

      havePlayStoreApp_: Boolean,

      showAndroidApps_: Boolean,

      showArcvmManageUsb_: Boolean,

      showCrostini_: Boolean,

      showToolbar_: Boolean,

      showNavMenu_: Boolean,

      showPluginVm_: Boolean,

      showReset_: Boolean,

      showStartup_: Boolean,

      showKerberosSection_: Boolean,

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
  private pageVisibility_: OSPageVisibility;
  private havePlayStoreApp_: boolean;
  private showAndroidApps_: boolean;
  private showArcvmManageUsb_: boolean;
  private showCrostini_: boolean;
  private showToolbar_: boolean;
  private showNavMenu_: boolean;
  private showPluginVm_: boolean;
  private showReset_: boolean;
  private showStartup_: boolean;
  private showKerberosSection_: boolean;
  private narrowThreshold_: number;
  private activeRoute_: Route|null;
  private scrollEndDebouncer_: Debouncer|null;

  constructor() {
    super();

    /**
     * The route of the selected element in os-settings-menu. Stored here to
     * defer navigation until drawer animation completes.
     */
    this.activeRoute_ = null;

    this.scrollEndDebouncer_ = null;

    Router.getInstance().initializeRouteFromUrl();
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

    this.havePlayStoreApp_ = loadTimeData.getBoolean('havePlayStoreApp');
    this.showAndroidApps_ = loadTimeData.getBoolean('androidAppsVisible');
    this.showArcvmManageUsb_ = loadTimeData.getBoolean('showArcvmManageUsb');
    this.showCrostini_ = loadTimeData.getBoolean('showCrostini');
    this.showPluginVm_ = loadTimeData.getBoolean('showPluginVm');
    this.showNavMenu_ = !loadTimeData.getBoolean('isKioskModeActive');
    this.showToolbar_ = !loadTimeData.getBoolean('isKioskModeActive');
    this.showReset_ = loadTimeData.getBoolean('allowPowerwash');
    this.showStartup_ = loadTimeData.getBoolean('showStartup');

    this.showKerberosSection_ = loadTimeData.valueExists('isKerberosEnabled') &&
        loadTimeData.getBoolean('isKerberosEnabled');

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });

    this.addEventListener('refresh-pref', this.onRefreshPref_);
    this.addEventListener('user-action-setting-change', this.onSettingChange_);

    // If navigation menu is not shown, do not listen to the drawer.
    if (!this.showNavMenu_) {
      return;
    }

    microTask.run(() => {
      // Lazy-create the drawer the first time it is opened or swiped into
      // view.
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

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    setTimeout(function() {
      chrome.send(
          'metricsHandler:recordTime',
          ['Settings.TimeUntilInteractive', window.performance.now()]);
    });

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    setGlobalScrollTarget(this.$.container);

    const scrollToTop = (top: number) => new Promise<void>(resolve => {
      if (this.$.container.scrollTop === top) {
        resolve();
        return;
      }

      this.$.container.scrollTo({top: top, behavior: 'auto'});
      const onScroll = () => {
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

    // Clicks need to be captured because unlike focus/blur to the settings
    // window, a click's propagation can be stopped by child elements.
    window.addEventListener('click', recordClick, /*capture=*/ true);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('focus', recordPageFocus);
    window.removeEventListener('blur', recordPageBlur);
    window.removeEventListener('click', recordClick);
    Router.getInstance().resetRouteForTesting();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    if (oldRoute && newRoute !== oldRoute) {
      // Search triggers route changes and currentRouteChanged() is called
      // in attached() state which is extraneous for this metric.
      recordNavigation();
    }

    if (newRoute.depth <= 1) {
      // Main page uses scroll visibility to determine shadow.
      this.enableShadowBehavior(true);
    } else {
      // Sub-pages always show the top-container shadow.
      this.enableShadowBehavior(false);
      this.showDropShadows();
    }
  }

  // Override FindShortcutBehavior methods.
  override handleFindShortcut(modalContextOpen: boolean) {
    if (modalContextOpen || !this.showToolbar_) {
      return false;
    }
    const toolbar = this.getToolbar_();
    toolbar.getSearchField().showAndFocus();
    toolbar.getSearchField().getSearchInput().select();
    return true;
  }

  // Override FindShortcutBehavior methods.
  override searchInputHasFocus() {
    if (!this.showToolbar_) {
      return false;
    }

    return this.getToolbar_().getSearchField().isSearchFocused();
  }

  private getDrawer_(): CrDrawerElement {
    return castExists(this.shadowRoot!.querySelector('cr-drawer'));
  }

  private getToolbar_(): OsToolbarElement {
    return castExists(this.shadowRoot!.querySelector('os-toolbar'));
  }

  private onRefreshPref_(e: CustomEvent<string>) {
    return this.$.prefs.refresh(e.detail);
  }

  private onSettingChange_(e: CustomEvent<{prefKey: string, prefValue: any}>) {
    const {prefKey, prefValue} = e.detail;
    const settingMetric = convertPrefToSettingMetric(prefKey, prefValue);

    // New metrics for this setting pref have not yet been implemented.
    if (!settingMetric) {
      recordSettingChange();
      return;
    }

    recordSettingChange(settingMetric.setting, settingMetric.value);
  }

  /**
   * Called when a section is selected.
   */
  private onIronActivate_(e: CustomEvent<{selected: string}>) {
    assert(this.showNavMenu_);
    const section = e.detail.selected;
    const path = new URL(section).pathname;
    const route = Router.getInstance().getRouteForPath(path);
    assert(route, 'os-settings-menu has an entry with an invalid route.');
    this.activeRoute_ = route;

    if (this.isNarrow) {
      // If the onIronActivate event came from the drawer, close the drawer
      // and wait for the menu to close before navigating to |activeRoute_|.
      this.getDrawer_().close();
      return;
    }
    this.navigateToActiveRoute_();
  }

  private onMenuButtonTap_() {
    if (!this.showNavMenu_) {
      return;
    }
    this.getDrawer_().toggle();
  }

  /**
   * Navigates to |activeRoute_| if set. Used to delay navigation until after
   * animations complete to ensure focus ends up in the right place.
   */
  private navigateToActiveRoute_() {
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
  private onMenuClose_() {
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

  private onAdvancedOpenedInMainChanged_() {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  }

  private onAdvancedOpenedInMenuChanged_() {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  }

  private onNarrowChanged_() {
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
  private onDrawerIconClick_() {
    this.getDrawer_().cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-ui': OsSettingsUiElement;
  }
}

customElements.define(OsSettingsUiElement.is, OsSettingsUiElement);
