// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './back_forward_button.js';
import './reload_button.js';
import './location_bar.js';
import './split_tabs_button.js';
import './home_button.js';
import './battery_saver_button.js';
import './performance_intervention_button.js';
import './pinned_toolbar_actions.js';
import './extensions.js';
import './app_menu_button.js';
import './avatar_button.js';
import '/shared/icon_table.js';
import '/shared/icon_from_table.js';
import './icons.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {IconTable} from '/shared/icon_table.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl, EventDispositionFlag, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, IconUpdate, NavigationControlsState, NavigationControlsStateListenerHandle} from './browser_proxy.js';
import {setHasHelpBubble} from './toolbar_button.js';

// clang-format off
// Helper so tests can find what they needed when optimization is on.
// Exporting from this file, the rollup file, ensures that we test the
// same code that we ship in optimized builds.
import type {IconFromTableElement} from '/shared/icon_from_table.js';
import {
  AppMenuIconType,
  AppMenuSeverity,
  AvatarToolbarButtonState,
  ContentSettingImageType,
  ContextMenuType,
  FocusRequestTarget,
  LhsChipIdentifier,
  OmniboxTextColor,
  PageActionId,
  PageActionTrigger,
  PermissionAction,
  PermissionChipTheme,
  PermissionPromptStyle,
  SplitTabActiveLocation,
} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {IconType} from '/shared/icon_handle.mojom-webui.js';
import type {OmniboxAction, LocationBarState, PageActionState, PermissionChipState, PermissionDashboardState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PermissionChipElement} from '/shared/permission_chip.js';
import type {PermissionDashboardElement} from '/shared/permission_dashboard.js';

import {INVALID_FOCUS_REQUEST_HANDLE} from './browser_proxy.js';
import {AppMenuButtonElement} from './app_menu_button.js';
import {ContentSettingIconElement} from './content_setting_icon.js';
import {ContentSettingsIconsElement} from './content_settings_icons.js';
import type {ExtensionsElement} from './extensions.js';
import {LocationBarElement} from './location_bar.js';
import {LocationIconElement} from './location_icon.js';
import {PageActionIconElement} from './page_action_icon.js';
import {PageActionIconsElement} from './page_action_icons.js';
import type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';
import type {PinnedToolbarActionsElement} from './pinned_toolbar_actions.js';
import {PointerProxyImpl} from './pointer_proxy.js';
import type {PointerProxy} from './pointer_proxy.js';
import {ReadonlyOmniboxElement} from './readonly_omnibox.js';
import {AnimationTracker, ToolbarActionContainerMixin} from './toolbar_action_container_mixin.js';
import type {KeyedActionState, ToolbarActionContainerMixinInterface} from './toolbar_action_container_mixin.js';
import {ToolbarActionMixin} from './toolbar_action_mixin.js';
import type {ToolbarActionMixinInterface} from './toolbar_action_mixin.js';
import {getClickSourceType, getContextMenuSourceType, PressHandler} from './toolbar_button.js';
import {ToolbarChipButtonElement} from './toolbar_chip_button.js';
import {CrLazyIconset} from './cr_lazy_iconset.js';

import {IconsetMap} from '//resources/cr_elements/cr_icon/iconset_map.js';
import {getTrustedHTML} from '//resources/js/static_types.js';

// TODO(crbug.com/535392412): do not export these from app.ts, find a better place for them instead.
export {
  AnimationTracker,
  AppMenuButtonElement,
  AppMenuIconType,
  AppMenuSeverity,
  BrowserProxyImpl,
  ContextMenuType,
  ContentSettingIconElement,
  ContentSettingImageType,
  ContentSettingsIconsElement,
  CrLazyIconset,
  EventDispositionFlag,
  FocusRequestTarget,
  getClickSourceType,
  getContextMenuSourceType,
  getTrustedHTML,
  getTypedBoolean,
  getTypedInteger,
  hasInitialStateKey,
  IconTable,
  IconType,
  IconsetMap,
  INVALID_FOCUS_REQUEST_HANDLE,
  INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE,
  LhsChipIdentifier,
  LocationBarElement,
  LocationIconElement,
  OmniboxTextColor,
  PageActionIconElement,
  PageActionIconsElement,
  PageActionId,
  PageActionTrigger,
  PermissionAction,
  PermissionChipElement,
  PermissionChipTheme,
  PermissionPromptStyle,
  PointerProxyImpl,
  PressHandler,
  ReadonlyOmniboxElement,
  resetInitialStateForTesting,
  ToolbarActionContainerMixin,
  ToolbarActionMixin,
  ToolbarChipButtonElement,
  TrackedElementManager,
};
export type {
  ExtensionsElement,
  IconFromTableElement,
  KeyedActionState,
  LocationBarState,
  OmniboxAction,
  PageActionState,
  PermissionChipState,
  PermissionDashboardElement,
  PermissionDashboardState,
  PinnedToolbarActionElement,
  PinnedToolbarActionsElement,
  PointerProxy,
  ToolbarActionContainerMixinInterface,
  ToolbarActionMixinInterface,
  ToolbarFlatStateSchema,
};
// clang-format on

const TRACKED_ELEMENTS: Array<{selector: string, id: string}> = [
  {selector: '#back', id: 'kToolbarBackButtonElementId'},
  {selector: '#forward', id: 'kToolbarForwardButtonElementId'},
  {selector: '#reload', id: 'kReloadButtonElementId'},
  {selector: '#split-tabs', id: 'kToolbarSplitTabsToolbarButtonElementId'},
  {selector: '#location-bar', id: 'kLocationBarElementId'},
  {selector: '#home', id: 'kToolbarHomeButtonElementId'},
  {selector: '#app-menu', id: 'kToolbarAppMenuButtonElementId'},
  {selector: '#avatar', id: 'kToolbarAvatarButtonElementId'},
  {selector: '#battery-saver', id: 'kToolbarBatterySaverButtonElementId'},
  {
    selector: '#performance-intervention',
    id: 'kToolbarPerformanceInterventionButtonElementId',
  },
];

const AppElementBase = HelpBubbleMixinLit(CrLitElement);

/**
 * Keys corresponding to initial toolbar state values provided by the browser.
 */
enum ToolbarStateKey {
  IS_NAVIGATION_LOADING = 'isNavigationLoading',
  RELOAD_CAN_SHOW_MENU = 'reloadCanShowMenu',
  BACK_BUTTON_ENABLED = 'backButtonEnabled',
  FORWARD_BUTTON_ENABLED = 'forwardButtonEnabled',
  HOME_BUTTON_SHOULD_BE_SHOWN = 'homeButtonShouldBeShown',
  BATTERY_SAVER_BUTTON_VISIBLE = 'batterySaverButtonVisible',
  LAYOUT_CONSTANTS_VERSION = 'layoutConstantsVersion',
  TOUCH_UI = 'touchUi',
  INITIAL_WEBUI_SURFACE_SYNC_ENABLED = 'initialWebUISurfaceSyncEnabled',
  IS_FALLBACK_PREWARMING = 'isFallbackPrewarming',
}

/**
 * Schema mapping `ToolbarStateKey` entries to their respective value types in
 * the initial state dictionary.
 */
interface ToolbarFlatStateSchema {
  [ToolbarStateKey.IS_NAVIGATION_LOADING]: boolean;
  [ToolbarStateKey.RELOAD_CAN_SHOW_MENU]: boolean;
  [ToolbarStateKey.BACK_BUTTON_ENABLED]: boolean;
  [ToolbarStateKey.FORWARD_BUTTON_ENABLED]: boolean;
  [ToolbarStateKey.HOME_BUTTON_SHOULD_BE_SHOWN]: boolean;
  [ToolbarStateKey.BATTERY_SAVER_BUTTON_VISIBLE]: boolean;
  [ToolbarStateKey.LAYOUT_CONSTANTS_VERSION]: number;
  [ToolbarStateKey.TOUCH_UI]: boolean;
  [ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED]: boolean;
  [ToolbarStateKey.IS_FALLBACK_PREWARMING]: boolean;
}

let parsedInitialState: Record<string, unknown>|null = null;

/**
 * Parses and returns the initial toolbar state dictionary from
 * `chrome.getVariableValue('initialState')`. Caches the parsed result after
 * the first call, returning an empty object if unparsable or absent.
 */
function getInitialState(): Record<string, unknown> {
  if (!parsedInitialState) {
    const jsonString = chrome.getVariableValue('initialState');
    if (jsonString) {
      try {
        parsedInitialState = JSON.parse(jsonString);
      } catch {
        parsedInitialState = {};
      }
    } else {
      parsedInitialState = {};
    }
  }
  return parsedInitialState!;
}

/**
 * Determines whether the specified `key` is present in the initial toolbar
 * state dictionary.
 */
function hasInitialStateKey(key: ToolbarStateKey): boolean {
  return key in getInitialState();
}

/**
 * Resets the cached initial toolbar state to `null` for testing purposes.
 */
function resetInitialStateForTesting(): void {
  parsedInitialState = null;
}

/**
 * Reads a boolean property for the given `key` from the initial state
 * dictionary. Returns `false` if the property is missing or not a boolean.
 */
function getTypedBoolean<K extends keyof ToolbarFlatStateSchema>(key: K):
    boolean {
  const state = getInitialState();
  const val = state[key];
  return typeof val === 'boolean' ? val : false;
}

/**
 * Reads a numeric property for the given `key` from the initial state
 * dictionary. Returns `0` if the property is missing or not a number.
 */
function getTypedInteger<K extends keyof ToolbarFlatStateSchema>(key: K):
    number {
  const state = getInitialState();
  const val = state[key];
  return typeof val === 'number' ? val : 0;
}

export class ToolbarAppElement extends AppElementBase {
  static get is() {
    return 'toolbar-app';
  }

  static override get styles() {
    return getCss();
  }

  /**
   * Returns the Lit element template. To prevent premature paint holding
   * resolution (FCP) during startup, we return `nothing` until the initial
   * navigation controls state has been received from the browser.
   */
  override render() {
    if (!this.isInitialized_) {
      return nothing;
    }
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isReloadButtonEnabled_: {type: Boolean},
      isAppMenuButtonEnabled_: {type: Boolean},
      isSplitTabsButtonEnabled_: {type: Boolean},
      isHomeButtonEnabled_: {type: Boolean},
      isBatterySaverButtonEnabled_: {type: Boolean},
      isLocationBarEnabled_: {type: Boolean},
      navigationControlsState_: {type: Object},
      isBackForwardButtonEnabled_: {type: Boolean},
      isPinnedToolbarActionsEnabled_: {type: Boolean},
      isExtensionsContainerEnabled_: {type: Boolean},
      isAvatarButtonEnabled_: {type: Boolean},
      isPerformanceInterventionButtonEnabled_: {type: Boolean},
      isInitialized_: {type: Boolean},
      isInitializedSyncForTesting_: {type: Boolean},
      initialSyncBootSuccess_: {type: Boolean},
    };
  }

  protected accessor isReloadButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableReloadButton');
  protected accessor isAppMenuButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableAppMenuButton');
  protected accessor isSplitTabsButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableSplitTabsButton');
  protected accessor isHomeButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableHomeButton');
  protected accessor isBatterySaverButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableBatterySaverButton');
  protected accessor isLocationBarEnabled_: boolean =
      loadTimeData.getBoolean('enableLocationBar');
  protected accessor isBackForwardButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableBackForwardButtons');
  protected accessor isPinnedToolbarActionsEnabled_: boolean =
      loadTimeData.getBoolean('enablePinnedToolbarActions');
  protected accessor isExtensionsContainerEnabled_: boolean =
      loadTimeData.getBoolean('enableExtensionsContainer');
  protected accessor isAvatarButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableAvatarButton');
  protected accessor isPerformanceInterventionButtonEnabled_: boolean =
      loadTimeData.getBoolean('enablePerformanceInterventionButton');
  /**
   * Tracks whether the element has received its first navigation state
   * update from the browser and completed its initial visual render.
   */
  protected accessor isInitialized_: boolean =
      !getTypedBoolean(ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED) ||
      hasInitialStateKey(ToolbarStateKey.IS_NAVIGATION_LOADING);
  // Test-only flag to verify that the toolbar was initialized synchronously.
  protected accessor isInitializedSyncForTesting_: boolean =
      hasInitialStateKey(ToolbarStateKey.IS_NAVIGATION_LOADING);
  protected accessor initialSyncBootSuccess_: boolean =
      hasInitialStateKey(ToolbarStateKey.IS_NAVIGATION_LOADING) &&
      hasInitialStateKey(ToolbarStateKey.BACK_BUTTON_ENABLED) &&
      hasInitialStateKey(ToolbarStateKey.FORWARD_BUTTON_ENABLED);
  protected accessor navigationControlsState_: NavigationControlsState = {
    reloadControlState: {
      // While this will be overwritten anyways, this matches the default value
      // on some platforms.
      doubleClickInterval: {microseconds: BigInt(500 * 1000)},

      canShowMenu: getTypedBoolean(ToolbarStateKey.RELOAD_CAN_SHOW_MENU),
      isNavigationLoading:
          getTypedBoolean(ToolbarStateKey.IS_NAVIGATION_LOADING),
      isContextMenuVisible: false,
      stateToken: 0,
    },
    splitTabsControlState: {
      isCurrentTabSplit: false,
      location: SplitTabActiveLocation.kStart,
      shouldBeShown: false,
      isContextMenuVisible: false,
    },
    backForwardControlState: {
      backButtonState: {
        enabled: getTypedBoolean(ToolbarStateKey.BACK_BUTTON_ENABLED),
        shouldBeShown: true,
        isContextMenuVisible: false,
      },
      forwardButtonState: {
        enabled: getTypedBoolean(ToolbarStateKey.FORWARD_BUTTON_ENABLED),
        shouldBeShown: true,
        isContextMenuVisible: false,
      },
      backButtonLeadingMargin: 0,
    },
    homeControlState: {
      shouldBeShown:
          getTypedBoolean(ToolbarStateKey.HOME_BUTTON_SHOULD_BE_SHOWN),
      isContextMenuVisible: false,
    },
    performanceInterventionControlState: {
      shouldBeShown: false,
      isActive: true,
    },
    appMenuControlState: {
      iconType: AppMenuIconType.kNone,
      severity: AppMenuSeverity.kNone,
      labelText: null,
      accessibilityText: '',
      tooltip: '',
      isContextMenuVisible: false,
      trailingMargin: 0,
    },

    batterySaverButtonVisible:
        getTypedBoolean(ToolbarStateKey.BATTERY_SAVER_BUTTON_VISIBLE),
    locationBarState: {
      omniboxViewState: {
        browserVersion: 0,
        uiVersion: 0,
        formattedFullUrl: '',
        textPieces: [],
        placeholder: null,
        inlineAutocompletion: '',
        additionalText: '',
        selection: null,
        textIsUrl: false,
        userInputInProgress: false,
      },
      locationBarFlags: {
        userInputInProgress: false,
        popupOpen: false,
        forceAimButtonFocusRing: false,
      },
      selectedKeyword: null,
      contentSettingImageStates: [],
      lhsChipsState: {
        securityChip: {
          icon: {handleId: 0n},
          securityLevel: 0,
          text: '',
          accessibilityState: {
            label: '',
            description: '',
          },
          isClickable: false,
          isTextDangerous: false,
          isVisible: true,
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
      pageActionStates: [],
    },
    avatarControlState: {
      state: AvatarToolbarButtonState.kNormal,
      icon: {handleId: 0n},
      text: '',
      tooltip: '',
      accessibilityName: '',
      accessibilityDescription: '',
      enabled: true,
      hasLinearGradientRing: false,
    },
    layoutConstantsVersion:
        getTypedInteger(ToolbarStateKey.LAYOUT_CONSTANTS_VERSION),
    touchUi: getTypedBoolean(ToolbarStateKey.TOUCH_UI),
    pinnedToolbarActionsState: [],
    extensionsState: [],
  };

  private browserProxy_: BrowserProxy;
  private navigationStateListenerHandle_:
      NavigationControlsStateListenerHandle =
          INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;
  private iconTable_: IconTable;
  private isPageInitialized_: boolean = false;
  private initializeSessionId_: number = 0;
  private dragOverListener_ = (e: DragEvent) => this.onDragOver_(e);
  private dropListener_ = (e: DragEvent) => this.onDrop_(e);
  private keyDownListener_ = (e: KeyboardEvent) => this.onKeyDown_(e);

  private isRtl_: boolean = loadTimeData.getString('textdirection') === 'rtl';

  protected readonly initialBootSnapshot_: {
    backButtonEnabled: boolean,
    forwardButtonEnabled: boolean,
    isNavigationLoading: boolean,
    initializedSync: boolean,
  };

  constructor() {
    super();
    this.initialBootSnapshot_ = Object.freeze({
      backButtonEnabled: getTypedBoolean(ToolbarStateKey.BACK_BUTTON_ENABLED),
      forwardButtonEnabled:
          getTypedBoolean(ToolbarStateKey.FORWARD_BUTTON_ENABLED),
      isNavigationLoading:
          getTypedBoolean(ToolbarStateKey.IS_NAVIGATION_LOADING),
      initializedSync: this.initialSyncBootSuccess_,
    });

    const initialWebUISurfaceSyncEnabled =
        getTypedBoolean(ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED);
    const isFallbackPrewarming =
        getTypedBoolean(ToolbarStateKey.IS_FALLBACK_PREWARMING);
    if (initialWebUISurfaceSyncEnabled && !isFallbackPrewarming) {
      assert(
          this.initialSyncBootSuccess_,
          'Sync startup expected but critical keys are missing!');
    }

    this.addEventListener('contextmenu', e => {
      // Suppress the default browser context menu (which includes "Inspect") to
      // align with native toolbar behavior. Any elements that require a
      // custom context menu are responsible for triggering their own menus.
      e.preventDefault();
    });
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.iconTable_ = IconTable.getInstance();
    ColorChangeUpdater.forDocument().start();
  }

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

    const sessionId = ++this.initializeSessionId_;

    this.addEventListener('dragover', this.dragOverListener_);
    this.addEventListener('drop', this.dropListener_);
    this.addEventListener('keydown', this.keyDownListener_);

    // Initial setup of CSS variables
    this.style.setProperty(
        '--split-tabs-indicator-width',
        `${loadTimeData.getInteger('splitTabsIndicatorWidth')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-height',
        `${loadTimeData.getInteger('splitTabsIndicatorHeight')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-spacing',
        `${loadTimeData.getInteger('splitTabsIndicatorSpacing')}px`);

    this.navigationStateListenerHandle_ =
        this.browserProxy_.addNavigationStateListener(
            (iconUpdates: IconUpdate[], state: NavigationControlsState) => {
              // This must be called before updating navigationControlsState_
              // so the new icons are available for rendering of child widgets.
              this.iconTable_.applyUpdates(iconUpdates);
              this.navigationControlsState_ = state;

              // Defer notifying the browser that the page is ready until after
              // the first Mojo-populated update has completed its render cycle.
              if (!this.isInitialized_) {
                this.isInitialized_ = true;
                this.updateComplete.then(() => {
                  this.initializePage_(sessionId);
                });
              }
            });

    if (this.isInitialized_) {
      this.updateComplete.then(() => {
        this.initializePage_(sessionId);
      });
    }
  }

  private initializePage_(sessionId: number) {
    if (sessionId !== this.initializeSessionId_ || !this.isConnected ||
        this.isPageInitialized_) {
      return;
    }
    this.isPageInitialized_ = true;

    for (const {selector, id} of TRACKED_ELEMENTS) {
      const el = this.shadowRoot.querySelector<HTMLElement>(selector);
      if (el) {
        this.registerHelpBubble(id, el, {
          onHighlightChanged: (highlighted: boolean) => {
            el.classList.toggle('anchor-highlight', highlighted);
          },
          onHelpBubbleShown: () => setHasHelpBubble(el, true),
          onHelpBubbleHidden: () => setHasHelpBubble(el, false),
        });
      }
    }

    const waitSelectors = [
      '#back',
      '#forward',
      '#reload',
      '#home',
      '#split-tabs',
      '#location-bar',
      '#extensions',
      '#pinnedToolbarActions',
      '#battery-saver',
      '#performance-intervention',
      '#avatar',
      '#app-menu',
    ];
    const promises =
        waitSelectors.map(s => this.shadowRoot.querySelector<CrLitElement>(s))
            .filter(el => !!el)
            .map(el => el.updateComplete);
    Promise.all(promises).then(() => {
      if (sessionId !== this.initializeSessionId_ || !this.isConnected) {
        return;
      }
      this.browserProxy_.toolbarUIHandler.onPageInitialized();
    });
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.removeEventListener('dragover', this.dragOverListener_);
    this.removeEventListener('drop', this.dropListener_);
    this.removeEventListener('keydown', this.keyDownListener_);

    this.browserProxy_.removeNavigationStateListener(
        this.navigationStateListenerHandle_);

    this.isInitialized_ =
        !getTypedBoolean(ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED) ||
        hasInitialStateKey(ToolbarStateKey.IS_NAVIGATION_LOADING);
    this.initializeSessionId_++;

    if (this.isPageInitialized_) {
      for (const {selector, id} of TRACKED_ELEMENTS) {
        const el = this.shadowRoot.querySelector<HTMLElement>(selector);
        if (el) {
          this.unregisterHelpBubble(id);
        }
      }
      this.isPageInitialized_ = false;
    }
  }

  // Drill down to find the actual active element
  private getDeepActiveElement(root: Document|ShadowRoot = document): Element
      |null {
    let active = root.activeElement;
    while (active && active.shadowRoot && active.shadowRoot.activeElement) {
      active = active.shadowRoot.activeElement;
    }
    return active;
  }

  // Recursively find all focusable elements
  private getDeepFocusableElements(root: Element|Document|ShadowRoot):
      HTMLElement[] {
    const focusableSelectors = 'button, cr-button, cr-icon-button, input';

    let focusable: HTMLElement[] = [];

    for (const node of Array.from(root.children)) {
      // 1. If this element matches our selectors, check if it's visible/enabled
      if (node.matches(focusableSelectors)) {
        const el = node as HTMLElement;
        const isDisabled = el.hasAttribute('disabled');
        const isHidden = el.closest('[hidden]') !== null;
        const isVisible = el.offsetWidth > 0 || el.offsetHeight > 0;

        if (!isDisabled && !isHidden && isVisible) {
          focusable.push(el);
        }

        // Don't bother digging into cr-buttons etc.
        continue;
      }

      // 2. If it has a Shadow DOM, pierce into it recursively
      if (node.shadowRoot) {
        focusable =
            focusable.concat(this.getDeepFocusableElements(node.shadowRoot));
      }

      // 3. Always check its Light DOM children as well (handles <slot>
      // projections)
      if (node.children.length > 0) {
        focusable = focusable.concat(this.getDeepFocusableElements(node));
      }
    }

    return focusable;
  }

  private onKeyDown_(event: KeyboardEvent) {
    if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight' &&
        // TODO(crbug.com/510825650): When app menu button enabled:
        // (event.key !== 'End' || !this.isAppMenuButtonEnabled_) &&
        (event.key !== 'Home' || !this.isBackForwardButtonEnabled_)) {
      return;
    }

    // Find focused element, may have to recurse in.
    const active =
        this.getDeepActiveElement(this.shadowRoot) as HTMLElement | null;
    if (!active) {
      return;
    }

    // Let omnibox handle these keys.
    if (active instanceof HTMLInputElement) {
      return;
    }

    // Build the array of targets.
    const focusableElements = this.getDeepFocusableElements(this.shadowRoot);

    const currentIndex = focusableElements.indexOf(active);
    assert(currentIndex !== -1);
    let nextIndex: number = 0;

    const shouldAdvance =
        event.key === (this.isRtl_ ? 'ArrowLeft' : 'ArrowRight');
    const shouldReverse =
        event.key === (this.isRtl_ ? 'ArrowRight' : 'ArrowLeft');

    if (event.key === 'Home') {
      nextIndex = 0;
      // TODO(crbug.com/510825650): When app menu button enabled:
      // } else if (event.key === 'End') {
      //   nextIndex = focusableElements.length - 1;
    } else if (shouldAdvance) {
      nextIndex = currentIndex + 1;
      // Let parent handle this for now.
      // TODO(crbug.com/510825650): Handle wrap around when app menu button is
      // WebUI.
      if (nextIndex >= focusableElements.length) {
        return;
      }
    } else if (shouldReverse) {
      nextIndex = currentIndex - 1;
      // Let parent handle this for now.
      // TODO(crbug.com/510825650): Handle wrap around when app menu button is
      // WebUI.
      if (nextIndex < 0) {
        return;
      }
    }

    event.preventDefault();
    focusableElements[nextIndex]!.focus();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const entry = performance.getEntriesByType('navigation')[0] as
        PerformanceNavigationTiming;
    if (entry) {
      chrome.histograms.recordTime(
          'InitialWebUI.Toolbar.ParseFinishedToFirstUpdate',
          Math.round(performance.now() - entry.domInteractive));
    }
  }

  protected onDragOver_(e: DragEvent) {
    if (e.dataTransfer &&
        (e.dataTransfer.types.includes('text/uri-list') ||
         e.dataTransfer.types.includes('text/plain') ||
         e.dataTransfer.types.includes('Files'))) {
      e.preventDefault();
      // By default, we show an allowed cursor over the general toolbar area.
      // Individual components (like the Omnibox) that handle their own
      // drag-and-drop will override this and call stopPropagation().
      e.dataTransfer.dropEffect = 'copy';
    }
  }

  protected onDrop_(e: DragEvent) {
    if (e.defaultPrevented) {
      return;
    }

    e.preventDefault();
    if (!e.dataTransfer) {
      return;
    }

    if (e.dataTransfer.types.includes('text/uri-list')) {
      const url = e.dataTransfer.getData('text/uri-list');
      if (url) {
        this.browserProxy_.browserControlsHandler.navigate(url.split('\n')[0]!);
      }
    } else if (e.dataTransfer.types.includes('Files')) {
      this.browserProxy_.toolbarUIHandler.onToolbarDropFile(
          {x: e.clientX, y: e.clientY});
    } else if (e.dataTransfer.types.includes('text/plain')) {
      const text = e.dataTransfer.getData('text/plain');
      if (text) {
        this.browserProxy_.browserControlsHandler.navigateText(text);
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
