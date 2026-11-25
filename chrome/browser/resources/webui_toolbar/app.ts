// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Register every toolbar iconset before any control is defined. Icon names
// come from icon_table.cc and can be handed to any <icon-from-table>, so a
// consuming element cannot know in advance which iconset it needs.
import './icons.js';
// <if expr="_google_chrome">
import './internal/icons.js';
// </if>
import '/shared/icon_from_table.js';
import '/shared/icon_table.js';
import './app_menu_button.js';
import './avatar_button.js';
import './back_forward_button.js';
import './battery_saver_button.js';
import './extensions.js';
import './glic_actor_task_icon.js';
import './glic_button.js';
import './home_button.js';
import './location_bar.js';
import './media_button.js';
import './overflow_button.js';
import './performance_intervention_button.js';
import './pinned_toolbar_actions.js';
import './reload_button.js';
import './split_tabs_button.js';
import './tab_focus_disabler.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {IconUpdate} from '/shared/icon_handle.mojom-webui.js';
import {IconTable} from '/shared/icon_table.js';
import {HighlightTracker, setHasHelpBubble} from '/shared/toolbar_button.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';
import {AppMenuIconType, AppMenuSeverity, AvatarToolbarButtonState, FocusRequestTarget, SplitTabActiveLocation} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {ToolbarState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl, createDefaultLocationBarState, INVALID_FOCUS_REQUEST_HANDLE, INVALID_TOOLBAR_STATE_LISTENER_HANDLE, ToolbarStateKey} from './browser_proxy.js';
import type {BoundProfileState, BrowserProxy, FocusRequestHandle, InitialStateReader, ToolbarStateListenerHandle} from './browser_proxy.js';
import type {ExtensionsElement} from './extensions.js';
import type {LocationBarElement} from './location_bar.js';
import type {OverflowButtonElement} from './overflow_button.js';
import type {ResponsiveControl} from './responsive_control.js';

let boundColorsStyleSheet: CSSStyleSheet|null = null;
let boundColorsCss = '';
let boundColorsAbortController: AbortController|null = null;

function clearBoundColorsCss() {
  boundColorsAbortController?.abort();
  boundColorsAbortController = null;
  if (boundColorsStyleSheet) {
    const sheet = boundColorsStyleSheet;
    boundColorsStyleSheet = null;
    document.adoptedStyleSheets =
        document.adoptedStyleSheets.filter(s => s !== sheet);
  }
  boundColorsCss = '';
}

// Set on `toolbar-app` while it shows an unbound spare's template state.
const AWAITING_BIND_ATTRIBUTE = 'awaiting-bind';

// Adopts `colorsCss` until the next `COLOR_PROVIDER_CHANGED` event. Returns
// false if `colorsCss` is empty or already applied.
function applyBoundColorsCss(colorsCss?: string): boolean {
  if (!colorsCss ||
      (colorsCss === boundColorsCss && boundColorsStyleSheet !== null &&
       document.adoptedStyleSheets.includes(boundColorsStyleSheet))) {
    return false;
  }
  const sheet = new CSSStyleSheet();
  sheet.replaceSync(colorsCss);
  clearBoundColorsCss();
  boundColorsCss = colorsCss;
  boundColorsStyleSheet = sheet;
  document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];

  const abortController = new AbortController();
  boundColorsAbortController = abortController;
  ColorChangeUpdater.forDocument().eventTarget.addEventListener(
      COLOR_PROVIDER_CHANGED, () => {
        if (boundColorsStyleSheet === sheet) {
          clearBoundColorsCss();
        }
      }, {once: true, signal: abortController.signal});
  return true;
}

// LINT.IfChange(InitialWebUIRendererMilestones)
const MARK_JS_RESOURCES_LOADED = 'JsResourcesLoaded';
const MARK_LOAD_TIME_DATA_READ = 'LoadTimeDataRead';
const MARK_JS_COMPOSITION_COMPLETE = 'JsCompositionComplete';
// LINT.ThenChange(//chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.cc:InitialWebUIRendererMilestones)

const TRACKED_ELEMENTS: Array<{selector: string, id: string}> = [
  {selector: '#back', id: 'kToolbarBackButtonElementId'},
  {selector: '#forward', id: 'kToolbarForwardButtonElementId'},
  {selector: '#reload', id: 'kReloadButtonElementId'},
  {selector: '#split-tabs', id: 'kToolbarSplitTabsToolbarButtonElementId'},
  {selector: '#location-bar', id: 'kLocationBarElementId'},
  {selector: '#home', id: 'kToolbarHomeButtonElementId'},
  {selector: '#overflow', id: 'kToolbarOverflowButtonElementId'},
  {selector: '#battery-saver', id: 'kToolbarBatterySaverButtonElementId'},
  {
    selector: '#performance-intervention',
    id: 'kToolbarPerformanceInterventionButtonElementId',
  },
  {selector: '#media', id: 'kToolbarMediaButtonElementId'},
  {selector: '#glic-button', id: 'kGlicButtonElementId'},
  {selector: '#glic-actor-task-icon', id: 'kGlicActorTaskIconElementId'},
  {selector: '#avatar', id: 'kToolbarAvatarButtonElementId'},
  {selector: '#app-menu', id: 'kToolbarAppMenuButtonElementId'},
];

const AppElementBase = HelpBubbleMixinLit(CrLitElement);

export class ToolbarAppElement extends AppElementBase {
  // Tolerance used when comparing the toolbar's width to the window's, both to
  // decide whether controls fit and whether a new layout is needed, to allow
  // for sub-pixel roundoff (layout rounds widths to a fraction of a pixel, e.g.
  // 1/64 in Blink, and zoom can add more error). Small enough not to be
  // visible.
  static readonly LAYOUT_TOLERANCE_PX = 0.05;

  static get is() {
    return 'toolbar-app';
  }

  static override get styles() {
    return getCss();
  }

  /**
   * Returns the Lit element template. To prevent premature paint holding
   * resolution (FCP) during startup, we return `nothing` until the initial
   * toolbar state has been received from the browser.
   */
  override render() {
    if (!this.isInitialized_) {
      return nothing;
    }
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      toolbarState_: {type: Object},
      isGlicButtonEnabled_: {type: Boolean},
      isAvatarButtonEnabled_: {type: Boolean},
      isInitialized_: {type: Boolean},
      hasReceivedToolbarState_: {type: Boolean},
    };
  }

  protected readonly isReloadButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableReloadButton');
  protected readonly isAppMenuButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableAppMenuButton');
  protected readonly isSplitTabsButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableSplitTabsButton');
  protected readonly isHomeButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableHomeButton');
  protected readonly isBatterySaverButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableBatterySaverButton');
  protected readonly isLocationBarEnabled_: boolean =
      loadTimeData.getBoolean('enableLocationBar');
  protected readonly isBackForwardButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableBackForwardButtons');
  protected readonly isPinnedToolbarActionsEnabled_: boolean =
      loadTimeData.getBoolean('enablePinnedToolbarActions');
  protected readonly isExtensionsContainerEnabled_: boolean =
      loadTimeData.getBoolean('enableExtensionsContainer');
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private initialState_: InitialStateReader =
      this.browserProxy_.getInitialState();
  protected accessor isGlicButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableGlicButton') &&
      this.initialState_.getBoolean(
          ToolbarStateKey.IS_PROFILE_ELIGIBLE_FOR_GLIC);
  protected accessor isAvatarButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableAvatarButton') &&
      this.initialState_.getBoolean(
          ToolbarStateKey.CAN_SHOW_AVATAR_FOR_PROFILE);
  protected readonly isPerformanceInterventionButtonEnabled_: boolean =
      loadTimeData.getBoolean('enablePerformanceInterventionButton');
  // <if expr="is_win or is_macosx or is_linux">
  protected readonly isMediaButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableMediaButton');
  // </if>
  /**
   * Tracks whether the element has received its first toolbar state
   * update from the browser and completed its initial visual render.
   */
  private accessor isInitialized_: boolean =
      !this.initialState_.getBoolean(
          ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED) ||
      this.initialState_.hasKey(ToolbarStateKey.IS_NAVIGATION_LOADING);
  private omniboxResizingPrioritizationEnabled_: boolean =
      loadTimeData.getBoolean('omniboxResizingPrioritizationEnabled');
  private toolbarAppMenuLabelResizingEnabled_: boolean =
      loadTimeData.getBoolean('toolbarAppMenuLabelResizingEnabled');
  private toolbarGlicButtonResizingEnabled_: boolean =
      loadTimeData.getBoolean('toolbarGlicButtonResizingEnabled');
  protected readonly webUIToolbarFullyEnabled_: boolean =
      loadTimeData.getBoolean('webUIToolbarFullyEnabled');
  protected accessor toolbarState_: ToolbarState = {
    reloadControlState: {
      // While this will be overwritten anyways, this matches the default value
      // on some platforms.
      doubleClickInterval: {microseconds: BigInt(500 * 1000)},

      canShowMenu: this.initialState_.getBoolean(
          ToolbarStateKey.RELOAD_CAN_SHOW_MENU),
      isNavigationLoading: this.initialState_.getBoolean(
          ToolbarStateKey.IS_NAVIGATION_LOADING),
      isContextMenuVisible: false,
      stateToken: 0,
    },
    splitTabsControlState: {
      isCurrentTabSplit: false,
      location: SplitTabActiveLocation.kStart,
      shouldBeShown: false,
      isContextMenuVisible: false,
      menuOpenToken: 0,
    },
    backForwardControlState: {
      backButtonState: {
        enabled: this.initialState_.getBoolean(
            ToolbarStateKey.BACK_BUTTON_ENABLED),
        shouldBeShown: true,
        isContextMenuVisible: false,
      },
      forwardButtonState: {
        enabled: this.initialState_.getBoolean(
            ToolbarStateKey.FORWARD_BUTTON_ENABLED),
        shouldBeShown: true,
        isContextMenuVisible: false,
      },
      windowIsMaximizedOrFullscreen: false,
    },
    homeControlState: {
      shouldBeShown: this.initialState_.getBoolean(
          ToolbarStateKey.HOME_BUTTON_SHOULD_BE_SHOWN),
      isContextMenuVisible: false,
    },
    performanceInterventionControlState: {
      shouldBeShown: false,
    },
    appMenuControlState: {
      iconType: AppMenuIconType.kNone,
      severity: AppMenuSeverity.kNone,
      labelText: null,
      accessibilityText: '',
      tooltip: '',
      isContextMenuVisible: false,
      windowIsMaximizedOrFullscreen: false,
    },

    batterySaverControlState: {
      shouldBeShown: this.initialState_.getBoolean(
          ToolbarStateKey.BATTERY_SAVER_BUTTON_VISIBLE),
      preventOverflow: false,
    },
    locationBarState: createDefaultLocationBarState(),
    avatarControlState: {
      state: AvatarToolbarButtonState.kNormal,
      icon: {handleId: 0n},
      text: '',
      tooltip: '',
      accessibilityName: '',
      accessibilityDescription: '',
      enabled: true,
      hasLinearGradientRing: false,
      preventOverflow: false,
    },
    overflowButtonControlState: {
      isContextMenuVisible: false,
    },
    mediaControlState: {
      enabled: true,
      shouldBeShown: false,
      isContextMenuVisible: false,
      preventOverflow: false,
    },
    glicButtonState: {
      open: false,
      shouldShow: false,
      isContextMenuVisible: false,
      shouldMatchToolbarStyle: false,
      useCustomThemeFallback: false,
      nudgeLabel: null,
      isSplit: false,
    },
    glicActorTaskIconState: {
      shouldShow: false,
      nudgeLabel: null,
      tooltip: '',
      isPressed: false,
    },
    layoutConstantsVersion: this.initialState_.getInteger(
        ToolbarStateKey.LAYOUT_CONSTANTS_VERSION),
    touchUi: this.initialState_.getBoolean(ToolbarStateKey.TOUCH_UI),
    pinnedToolbarActionsState: [],
    extensionsState: [],
    controlsAppearInactive: false,
    keyboardNavigable: this.initialState_.getBoolean(
        ToolbarStateKey.KEYBOARD_NAVIGABLE),
  };
  private toolbarStateListenerHandle_: ToolbarStateListenerHandle =
      INVALID_TOOLBAR_STATE_LISTENER_HANDLE;
  private focusRequestHandle_: FocusRequestHandle =
      INVALID_FOCUS_REQUEST_HANDLE;
  private iconTable_: IconTable;
  private isPageInitialized_: boolean = false;
  private registeredTrackedElements_ = new Map<string, HTMLElement>();
  private hasStartedRegisteringTrackedElements_ = false;
  private isColorChangeUpdaterStarted_ = false;
  private isBoundProfileApplied_ = false;
  private hasReadState_ = false;
  // Set when the browser's first ToolbarState (the
  // ToolbarUIService.getInitialState() answer) arrives, and reset when this
  // element is disconnected. Reactive because the template derives the location
  // bar's waitForState from it.
  protected accessor hasReceivedToolbarState_: boolean = false;
  private initializeSessionId_: number = 0;
  private resizeObserver_?: ResizeObserver;

  // Indicates whether there's a pending call to layoutIfNeeded_() on the next
  // animation frame, so another one will not be queued.
  private pendingLayoutIfNeeded_: boolean = false;
  // Indicates whether a layout needs to be done. If set, the next
  // layoutIfNeeded_() call will always do a layout, unless
  // layoutResponsiveControls() is called directly in the meantime. Any code
  // that sets this to true should also schedule a layoutIfNeeded_() call.
  private pendingLayout_: boolean = false;

  private dragOverListener_ = (e: DragEvent) => this.onDragOver_(e);
  private dropListener_ = (e: DragEvent) => this.onDrop_(e);
  private keyDownListener_ = (e: KeyboardEvent) => this.onKeyDown_(e);
  private windowResizeListener_:
      () => void = () => this.scheduleLayoutIfNeeded_();
  private requestLayoutListener_:
      () => void = () => this.scheduleLayoutResponsiveControls_();
  private layoutNowListener_:
      () => void = () => this.layoutResponsiveControls();

  private isRtl_: boolean = loadTimeData.getString('textdirection') === 'rtl';

  get browserProxyForTesting(): BrowserProxy {
    return this.browserProxy_;
  }

  readonly initialBootSnapshotForTesting: {
    backButtonEnabled: boolean,
    forwardButtonEnabled: boolean,
    isNavigationLoading: boolean,
    initializedSync: boolean,
  };

  constructor() {
    super();
    const initialSyncBootSuccess =
        this.initialState_.hasKey(ToolbarStateKey.IS_NAVIGATION_LOADING) &&
        this.initialState_.hasKey(ToolbarStateKey.BACK_BUTTON_ENABLED) &&
        this.initialState_.hasKey(ToolbarStateKey.FORWARD_BUTTON_ENABLED);
    this.initialBootSnapshotForTesting = Object.freeze({
      backButtonEnabled:
          this.initialState_.getBoolean(ToolbarStateKey.BACK_BUTTON_ENABLED),
      forwardButtonEnabled:
          this.initialState_.getBoolean(ToolbarStateKey.FORWARD_BUTTON_ENABLED),
      isNavigationLoading:
          this.initialState_.getBoolean(ToolbarStateKey.IS_NAVIGATION_LOADING),
      initializedSync: initialSyncBootSuccess,
    });

    const initialWebUISurfaceSyncEnabled = this.initialState_.getBoolean(
        ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED);
    const isFallbackPrewarming =
        this.initialState_.getBoolean(ToolbarStateKey.IS_FALLBACK_PREWARMING);
    if (initialWebUISurfaceSyncEnabled && !isFallbackPrewarming) {
      assert(
          initialSyncBootSuccess,
          'Sync startup expected but critical keys are missing!');
    }

    this.addEventListener('contextmenu', e => {
      // Suppress the default browser context menu (which includes "Inspect") to
      // align with native toolbar behavior. Any elements that require a
      // custom context menu are responsible for triggering their own menus.
      e.preventDefault();
    });
    this.iconTable_ = IconTable.getInstance();
    // An unbound spare, or a spare bound before commit with `colorsCss`,
    // starts this when it applies the bound state.
    if (!this.initialState_.getBoolean(ToolbarStateKey.IS_UNBOUND) &&
        !this.initialState_.getString(ToolbarStateKey.COLORS_CSS)) {
      this.startColorChangeUpdater_();
    }
    MetricsReporterImpl.getInstance().mark(MARK_JS_RESOURCES_LOADED);
  }

  // Starts `colors.css` updates. An unbound spare defers this until a window
  // adopts it, so the first `COLOR_PROVIDER_CHANGED` event follows a
  // `colors.css` load for the bound profile, not for the template profile.
  private startColorChangeUpdater_() {
    if (this.isColorChangeUpdaterStarted_) {
      return;
    }
    this.isColorChangeUpdaterStarted_ = true;
    ColorChangeUpdater.forDocument().start();
  }

  /**
   * Applies the user profile's theme colors CSS and button eligibility
   * synchronously when an unbound toolbar spare is adopted by a window.
   */
  applyBoundProfile(state: BoundProfileState) {
    const colorsApplied = applyBoundColorsCss(state.colorsCss);
    if (!this.isColorChangeUpdaterStarted_) {
      this.startColorChangeUpdater_();
    } else if (colorsApplied) {
      // The `colors.css` load that start() triggered may have finished
      // already, so load it again to retire the adopted sheet.
      ColorChangeUpdater.forDocument().onColorProviderChanged();
    }

    let controlsChanged = false;
    if (state.isProfileEligibleForGlic !== undefined) {
      const nextGlic = loadTimeData.getBoolean('enableGlicButton') &&
          state.isProfileEligibleForGlic;
      if (this.isGlicButtonEnabled_ !== nextGlic) {
        this.isGlicButtonEnabled_ = nextGlic;
        controlsChanged = true;
      }
    }
    if (state.canShowAvatarForProfile !== undefined) {
      const nextAvatar = loadTimeData.getBoolean('enableAvatarButton') &&
          state.canShowAvatarForProfile;
      if (this.isAvatarButtonEnabled_ !== nextAvatar) {
        this.isAvatarButtonEnabled_ = nextAvatar;
        controlsChanged = true;
        // The first toolbar state clears `initial-load` on the avatar it
        // renders, so an avatar added after it clears its own.
        if (nextAvatar && this.hasReceivedToolbarState_) {
          this.updateComplete.then(() => this.clearAvatarInitialLoad_());
        }
      }
    }
    if (!this.hasReceivedToolbarState_ &&
        state.homeButtonShouldBeShown !== undefined &&
        this.toolbarState_.homeControlState.shouldBeShown !==
            state.homeButtonShouldBeShown) {
      this.toolbarState_ = {
        ...this.toolbarState_,
        homeControlState: {
          ...this.toolbarState_.homeControlState,
          shouldBeShown: state.homeButtonShouldBeShown,
        },
      };
      controlsChanged = true;
    }
    // Show the content now that the bound state is applied. Lit renders the
    // changes above in a microtask, before the next frame.
    this.isBoundProfileApplied_ = true;
    this.removeAttribute(AWAITING_BIND_ATTRIBUTE);
    this.inert = false;
    if (!controlsChanged) {
      return;
    }
    this.scheduleLayoutResponsiveControls_();
    if (this.hasStartedRegisteringTrackedElements_) {
      const sessionId = this.initializeSessionId_;
      this.updateComplete.then(() => this.registerTrackedElements_(sessionId));
    }
  }

  // Removes `initial-load` from the avatar button once it has rendered, so
  // later changes animate.
  private async clearAvatarInitialLoad_() {
    const avatar = this.shadowRoot.querySelector<CrLitElement>('#avatar');
    if (!avatar) {
      return;
    }
    await avatar.updateComplete;
    requestAnimationFrame(() => {
      avatar.classList.remove('initial-load');
    });
  }

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

    if (this.webUIToolbarFullyEnabled_) {
      this.setAttribute('webui-toolbar-fully-enabled', '');
    }
    const sessionId = ++this.initializeSessionId_;

    this.setAttribute('role', 'toolbar');

    this.addEventListener('dragover', this.dragOverListener_);
    this.addEventListener('drop', this.dropListener_);
    this.addEventListener('keydown', this.keyDownListener_);
    this.addEventListener('request-layout', this.requestLayoutListener_);
    this.addEventListener('layout-now', this.layoutNowListener_);

    // This ResizeObserver performs a new layout, if needed, when toolbar-app is
    // resized. This is too late in the process to avoid a visible re-layout,
    // but serves to make sure if there's any path where we resize elements but
    // fail to call one of the other methods that triggers a new layout, we'll
    // still do a layout.
    //
    // Since this method does nothing if window size and the toolbar size match,
    // this is a fairly low overhead call, if we don't need to do a new layout.
    this.resizeObserver_ =
        new ResizeObserver(() => this.scheduleLayoutIfNeeded_());
    this.resizeObserver_.observe(this);
    window.addEventListener('resize', this.windowResizeListener_);

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

    this.toolbarStateListenerHandle_ =
        this.browserProxy_.addToolbarStateListener(
            (iconUpdates: IconUpdate[], state: ToolbarState) => {
              // This must be called before updating toolbarState_
              // so the new icons are available for rendering of child widgets.
              this.iconTable_.applyUpdates(iconUpdates);
              this.toolbarState_ = state;

              if (!this.hasReadState_) {
                this.hasReadState_ = true;
                MetricsReporterImpl.getInstance().mark(
                    MARK_LOAD_TIME_DATA_READ);
              }

              const isFirstToolbarState = !this.hasReceivedToolbarState_;
              if (isFirstToolbarState) {
                this.hasReceivedToolbarState_ = true;
                this.updateComplete.then(async () => {
                  const extensions =
                      this.shadowRoot.querySelector<ExtensionsElement>(
                          '#extensions');
                  await Promise.all([
                    this.clearAvatarInitialLoad_(),
                    (async () => {
                      if (extensions) {
                        await extensions.updateComplete;
                        requestAnimationFrame(() => {
                          extensions.markInitialStateRendered();
                        });
                      }
                    })(),
                  ]);
                });

                this.focusRequestHandle_ =
                    this.browserProxy_.addFocusRequestListener(
                        this.onFocusRequest_.bind(this));
              }

              // Defer notifying the browser that the page is ready until after
              // the first Mojo-populated update has completed its render cycle.
              if (!this.isInitialized_) {
                this.isInitialized_ = true;
                this.updateComplete.then(() => {
                  MetricsReporterImpl.getInstance().mark(
                      MARK_JS_COMPOSITION_COMPLETE);
                  this.initializePage_(sessionId);
                });
              }

              // This message may result in microtasks that ultimately end up
              // resizing the toolbar. Call scheduleLayoutIfNeeded_() to
              // schedule a check if a new layout is needed to adjust the
              // toolbar's size to correct for any newly shown/hidden controls
              // that are not ResponsiveControls. The task will run right before
              // the next render frame, after all microtasks that might affect
              // layout have been executed.
              this.scheduleLayoutIfNeeded_();
            });

    if (this.isInitialized_) {
      this.updateComplete.then(() => {
        MetricsReporterImpl.getInstance().mark(MARK_JS_COMPOSITION_COMPLETE);
        this.initializePage_(sessionId);
      });
    }

    if (this.initialState_.getBoolean(ToolbarStateKey.IS_UNBOUND)) {
      // Keep the template profile's state off screen and non-interactive
      // until the adopting window's bind message is applied.
      if (!this.isBoundProfileApplied_) {
        this.setAttribute(AWAITING_BIND_ATTRIBUTE, '');
        this.inert = true;
      }
      window.__webuiToolbarApplyBoundProfile = (state: BoundProfileState) =>
          this.applyBoundProfile(state);
      // The browser stashes the bound state here if a window adopted this
      // spare after commit but before this script ran.
      const stashedState = window.__webuiToolbarBoundProfile;
      if (stashedState) {
        delete window.__webuiToolbarBoundProfile;
        this.applyBoundProfile(stashedState);
      }
    } else {
      // A window that adopted this spare before commit overwrote
      // `initialState`, adding `colorsCss` if its colors differ.
      const colorsCss =
          this.initialState_.getString(ToolbarStateKey.COLORS_CSS);
      if (colorsCss && !this.isBoundProfileApplied_) {
        this.applyBoundProfile({colorsCss});
      }
      if (colorsCss ||
          this.initialState_.getBoolean(
              ToolbarStateKey.IS_FALLBACK_PREWARMING)) {
        // `DidFinishNavigation()` still sends the bound diff at commit in
        // case the page read the unbound `initialState` before the
        // overwrite landed; ignore it here without stashing it on `window`.
        delete window.__webuiToolbarBoundProfile;
        window.__webuiToolbarApplyBoundProfile = () => {};
      }
    }
  }

  private registerTrackedElements_(sessionId: number): void {
    if (sessionId !== this.initializeSessionId_ || !this.isConnected) {
      return;
    }

    for (const {selector, id} of TRACKED_ELEMENTS) {
      const el =
          this.shadowRoot.querySelector<HTMLElement>(selector) ?? undefined;
      const registeredEl = this.registeredTrackedElements_.get(id);
      if (el === registeredEl) {
        continue;
      }
      // A control that applyBoundProfile() removed or re-rendered.
      if (registeredEl) {
        this.unregisterHelpBubble(id);
        this.registeredTrackedElements_.delete(id);
      }
      if (el) {
        this.registeredTrackedElements_.set(id, el);
        const tracker = (el as {highlightTracker?: unknown}).highlightTracker;
        this.registerHelpBubble(id, el, {
          onHighlightChanged: (highlighted: boolean) => {
            el.classList.toggle('anchor-highlight', highlighted);
            if (tracker instanceof HighlightTracker) {
              tracker.onHighlightChanged(highlighted);
            }
          },
          onHelpBubbleShown: () => setHasHelpBubble(el, true),
          onHelpBubbleHidden: () => setHasHelpBubble(el, false),
        });
      }
    }
  }

  private async initializePage_(sessionId: number) {
    if (sessionId !== this.initializeSessionId_ || !this.isConnected ||
        this.isPageInitialized_) {
      return;
    }

    this.hasStartedRegisteringTrackedElements_ = true;
    this.registerTrackedElements_(sessionId);
    this.isPageInitialized_ = true;

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
      '#glic-button',
      '#glic-actor-task-icon',
      '#avatar',
      '#media',
      '#overflow',
      '#app-menu',
    ];
    const promises =
        waitSelectors.map(s => this.shadowRoot.querySelector<CrLitElement>(s))
            .filter(el => !!el)
            .map(el => el.updateComplete);
    await Promise.all(promises);
    if (sessionId !== this.initializeSessionId_ || !this.isConnected) {
      return;
    }
    this.layoutIfNeeded_();
    this.browserProxy_.toolbarUIHandler.onPageInitialized();
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    delete window.__webuiToolbarApplyBoundProfile;
    clearBoundColorsCss();

    this.removeEventListener('dragover', this.dragOverListener_);
    this.removeEventListener('drop', this.dropListener_);
    this.removeEventListener('keydown', this.keyDownListener_);
    this.removeEventListener('request-layout', this.requestLayoutListener_);
    this.removeEventListener('layout-now', this.layoutNowListener_);

    this.resizeObserver_?.disconnect();
    window.removeEventListener('resize', this.windowResizeListener_);

    this.browserProxy_.removeToolbarStateListener(
        this.toolbarStateListenerHandle_);
    if (this.focusRequestHandle_ !== INVALID_FOCUS_REQUEST_HANDLE) {
      this.browserProxy_.removeFocusRequestListener(this.focusRequestHandle_);
    }

    this.initialState_ = this.browserProxy_.getInitialState();
    this.isInitialized_ =
        !this.initialState_.getBoolean(
            ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED) ||
        this.initialState_.hasKey(ToolbarStateKey.IS_NAVIGATION_LOADING);
    this.hasReceivedToolbarState_ = false;
    this.initializeSessionId_++;

    for (const id of this.registeredTrackedElements_.keys()) {
      this.unregisterHelpBubble(id);
    }
    this.registeredTrackedElements_.clear();
    this.hasStartedRegisteringTrackedElements_ = false;
    this.isPageInitialized_ = false;
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

  private isNonFocusable_(el: Element) {
    // Note: we need to check tabindex as attribute and not property
    // here since a missing tabindex for things that aren't inherently
    // tabbable returns -1 for property, but can behave differently.
    return el.getAttribute('tabindex') === '-1';
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
        const isVisible = el.offsetWidth > 0 && el.offsetHeight > 0;
        const nonFocusable = this.isNonFocusable_(el);

        if (!isDisabled && !isHidden && !nonFocusable && isVisible) {
          focusable.push(el);
        }

        // Don't bother digging into cr-buttons etc.
        continue;
      }

      if (node.localName === 'tab-focus-disabler' &&
          this.isNonFocusable_(node)) {
        // If we group disabled tab navigation for a portion of toolbar,
        // don't recurse there.
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
        (event.key !== 'End' || !this.isAppMenuButtonEnabled_) &&
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
    } else if (event.key === 'End') {
      nextIndex = focusableElements.length - 1;
    } else if (shouldAdvance) {
      nextIndex = currentIndex + 1;
      if (nextIndex >= focusableElements.length) {
        // If full WebUI toolbar, handle wrap around.
        if (this.isBackForwardButtonEnabled_ && this.isAppMenuButtonEnabled_) {
          nextIndex = 0;
        } else {
          // Let parent handle this for now.
          return;
        }
      }
    } else if (shouldReverse) {
      nextIndex = currentIndex - 1;
      if (nextIndex < 0) {
        // If full WebUI toolbar, handle wrap around.
        if (this.isBackForwardButtonEnabled_ && this.isAppMenuButtonEnabled_) {
          nextIndex = focusableElements.length - 1;
        } else {
          // Let parent handle this for now.
          return;
        }
      }
    }

    event.preventDefault();
    focusableElements[nextIndex]!.focus();
  }

  private onFocusRequest_(target: FocusRequestTarget) {
    if (target === FocusRequestTarget.kFirstFocusable) {
      // updateComplete won't catch the update of tabIndex of
      // location bar chips.
      requestAnimationFrame(() => this.focusFirst_());
    }
  }

  private focusFirst_() {
    const focusableElements = this.getDeepFocusableElements(this.shadowRoot);
    if (focusableElements.length) {
      focusableElements[0]!.focus({focusVisible: true});
    } else {
      this.browserProxy_.toolbarUIHandler.focusOut(/*reverse=*/ false);
    }
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('toolbarState_')) {
      this.toggleAttribute(
          'controls-appear-inactive',
          this.toolbarState_.controlsAppearInactive);
    }
  }

  /**
   * Schedules a layoutIfNeeded_() on the next animation frame, if one isn't
   * already scheduled. layoutIfNeeded_() will layout the ResponsiveControls if
   * toolbarWidthMatchesWindowWidth() is false (so a new layout is needed), or
   * if `pendingLayout_` is true.
   *
   * The scheduled task should be fast if no layout is actually needed.
   *
   * Called on window resize, toolbar-app resize, or when something happens that
   * may result in resizing the toolbar-app. Also used by
   * scheduleLayoutResponsiveControls_().
   */
  private scheduleLayoutIfNeeded_() {
    if (!this.webUIToolbarFullyEnabled_ || this.pendingLayoutIfNeeded_) {
      return;
    }
    this.pendingLayoutIfNeeded_ = true;
    requestAnimationFrame(() => this.layoutIfNeeded_());
  }

  /**
   * Schedules a layout of responsive controls before the next animation frame.
   * Multiple calls are aggregated into a single layout, so this should
   * typically be used rather than calling layoutResponsiveControls() directly.
   *
   * This should be called whenever any control's visibility state or size
   * changes, and a new layout is definitely needed. ResponsiveControls
   * themselves are responsible for bubbling up `request-layout` events to the
   * app to trigger layouts when their preferred and/or minimize sizes may have
   * changed, while non-responsive controls are monitored by watching the size
   * of the toolbar-app element itself. If two or more non-ResponsiveControls
   * change in size in such a way that the toolbar's size remains the same, no
   * resize is triggered, which is fine, since that should have no effect on
   * which ResponsiveControls should be displayed.
   */
  private scheduleLayoutResponsiveControls_() {
    if (!this.webUIToolbarFullyEnabled_ || this.pendingLayout_) {
      return;
    }
    this.pendingLayout_ = true;
    this.scheduleLayoutIfNeeded_();
  }

  private layoutIfNeeded_() {
    this.pendingLayoutIfNeeded_ = false;
    if (!this.isConnected) {
      this.pendingLayout_ = false;
      return;
    }

    if (this.pendingLayout_ || !this.toolbarWidthMatchesWindowWidth()) {
      // Note that this will set `pendingLayout_` to false.
      this.layoutResponsiveControls();
    }
  }

  /**
   * Returns the amount of available width for the toolbar, in (possibly
   * fractional) pixels, which is the difference between the window inner width
   * and the current width of this element. This is intended to be used during
   * layout, which attempts to size controls so that there's no available width.
   *
   * Note that this value can be negative if the toolbar element's width exceeds
   * the window's inner width.
   *
   * Since the result is fractional, it should generally not be compared
   * directly against 0. Use toolbarWidthFitsInWindow() or
   * toolbarWidthMatchesWindowWidth() instead.
   */
  getAvailableWidth(): number {
    return window.innerWidth - this.getBoundingClientRect().width;
  }

  /**
   * Returns true if the toolbar, at its current width, fits in the window,
   * allowing it to be up to LAYOUT_TOLERANCE_PX wider than the window. The
   * toolbar's width depends on the current state of all controls, so this is
   * used during layout to check whether the controls sized so far fit.
   */
  toolbarWidthFitsInWindow(): boolean {
    return this.getAvailableWidth() >= -ToolbarAppElement.LAYOUT_TOLERANCE_PX;
  }

  /**
   * Returns true if the toolbar's current width matches the window's, to
   * within LAYOUT_TOLERANCE_PX. Like toolbarWidthFitsInWindow(), this depends
   * on the current state of all controls. If false when we're not actively
   * performing a layout, the toolbar needs to be laid out again.
   */
  toolbarWidthMatchesWindowWidth(): boolean {
    return Math.abs(this.getAvailableWidth()) <=
        ToolbarAppElement.LAYOUT_TOLERANCE_PX;
  }

  /**
   * Resizes / shows / hides responsive controls based on available space in the
   * toolbar. Returns true if all controls were successfully laid out; returns
   * false if `failOnOverflow` is true and any control overflowed.
   *
   * If true is returned, all available space on the window should be taken up
   * by the toolbar and its controls. If false is returned, the method has
   * exited early, and the toolbar may not be taking up all available space in
   * the window - the expectation is that this method will be called again,
   * after making the overflow button visible, without `failOnOverflow` set.
   */
  private runLayoutPass(
      responsiveControls: ResponsiveControl[], locationBar: LocationBarElement,
      failOnOverflow: boolean): boolean {
    // Set all responsive elements that should be shown to their minimum width.
    for (const control of responsiveControls) {
      // Controls that should not be shown are hidden by other means than
      // setting `overflow-display-none`, so they can be ignored entirely.
      if (control.shouldBeShown()) {
        control.setToMinWidth();
      }
    }

    // Try expanding each element to preferred width in order of priority.
    for (const control of responsiveControls) {
      if (!control.shouldBeShown()) {
        continue;
      }

      control.expandUpToPreferredWidth();

      if (failOnOverflow && control.controlsToAddToOverflowMenu().length > 0) {
        return false;
      }
    }

    // Assign all remaining space to the location bar.
    if (locationBar.shouldBeShown()) {
      locationBar.setToMaxAvailableWidth();
    }

    return true;
  }

  /**
   * Returns a prioritized Array of responsive controls that can be resized or
   * hidden so the toolbar fits in the window. Earlier controls in the Array
   * have higher priority.
   *
   * Controls that can't be hidden on overflow or otherwise change size in
   * response to the amount of available width are not ResponsiveControls, so
   * are not returned by this method. e.g., of the navigation buttons on the
   * left of the toolbar, the back and reload buttons are always shown, and
   * never moved to the overflow menu, so are not included.
   */
  getResponsiveControls(): Array<ResponsiveControl&HTMLElement> {
    const locationBar =
        this.shadowRoot.querySelector<LocationBarElement>('#location-bar')!;

    const buttons = [
      (this.toolbarAppMenuLabelResizingEnabled_ &&
       this.toolbarState_.appMenuControlState.labelText) ?
          this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
              '#app-menu') :
          null,
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>('#avatar'),
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
          '#split-tabs'),
      this.toolbarGlicButtonResizingEnabled_ ?
          this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
              '#glic-button') :
          null,
      this.toolbarGlicButtonResizingEnabled_ ?
          this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
              '#glic-actor-task-icon') :
          null,
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>('#forward'),
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>('#home'),
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
          '#battery-saver'),
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>('#media'),
    ];

    const groupedControls = [
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
          '#pinnedToolbarActions'),
      this.shadowRoot.querySelector<ResponsiveControl&HTMLElement>(
          '#extensions'),
    ];

    return (this.omniboxResizingPrioritizationEnabled_ ?
                [locationBar, ...buttons, ...groupedControls] :
                [...buttons, locationBar, ...groupedControls])
        .filter((el): el is ResponsiveControl&HTMLElement => el !== null);
  }

  /**
   * Returns information on all controls that are currently hidden due to
   * overflow and should therefore be displayed on the overflow menu.
   */
  getOverflowedMenuItems(): OverflowMenuItem[] {
    const overflowControls: OverflowMenuItem[] = [];
    for (const control of this.getResponsiveControls()) {
      overflowControls.push(...control.controlsToAddToOverflowMenu());
    }
    return overflowControls;
  }

  /**
   * Returns the name of the ui::ElementIdentifier of `element`, if it's one of
   * the controls in TRACKED_ELEMENTS. Unlike
   * TrackedElementManager.getElementId(), this works before `element` has been
   * registered with the TrackedElementManager.
   */
  getElementIdentifier(element: HTMLElement): string|undefined {
    return TRACKED_ELEMENTS
        .find(
            ({selector}) => this.shadowRoot.querySelector(selector) === element)
        ?.id;
  }

  /**
   * Resizes / shows / hides responsive controls based on available space in the
   * toolbar.
   *
   * This should generally only be called through
   * `scheduleLayoutResponsiveControls_`, to avoid redundant calls, which can
   * be fairly heavy weight.
   *
   * TODO(crbug.com/491791965): Investigate performance of this method. It does
   * force a lot of layouts, which may well be a performance issue. There are
   * ways to improve performance, at potentially significant complexity cost.
   */
  private layoutResponsiveControls() {
    this.pendingLayout_ = false;

    // If `webUIToolbarFullyEnabled_` is false, the C++ FlexLayout class will
    // handle laying out controls.
    if (!this.webUIToolbarFullyEnabled_) {
      return;
    }

    // Wait until the initial template has rendered (#location-bar and
    // #overflow exist in shadowRoot).
    if (!this.isInitialized_) {
      return;
    }

    const responsiveControls = this.getResponsiveControls();
    const locationBar =
        this.shadowRoot.querySelector<LocationBarElement>('#location-bar')!;
    const overflowButton =
        this.shadowRoot.querySelector<OverflowButtonElement>('#overflow')!;

    // If the overflow button was not shown before, set all controls to their
    // preferred widths and see if the toolbar then fits in the window. If so,
    // expand the location bar to take up any remaining width in the window, and
    // we're done.
    //
    // This is an optimization to avoid performing extra layouts in the common
    // case where all controls fit, since each expandUpToPreferredWidth() call
    // in runLayoutPass() triggers a layout, which can be expensive.
    if (overflowButton.hasAttribute('hidden')) {
      for (const control of responsiveControls) {
        if (control.shouldBeShown()) {
          control.setToPreferredWidth();
        }
      }
      if (this.toolbarWidthFitsInWindow()) {
        if (locationBar.shouldBeShown()) {
          locationBar.setToMaxAvailableWidth();
        }
        return;
      }
    }

    // Hide the overflow button and attempt to lay out all buttons, failing on
    // any overflow.
    overflowButton.toggleAttribute('hidden', true);
    if (!this.runLayoutPass(
            responsiveControls, locationBar, /*failOnOverflow=*/ true)) {
      // If any control overflowed, show overflow button and do another pass.
      overflowButton.toggleAttribute('hidden', false);
      this.runLayoutPass(
          responsiveControls, locationBar, /*failOnOverflow=*/ false);
    }
  }

  private onDragOver_(e: DragEvent) {
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

  private onDrop_(e: DragEvent) {
    if (e.defaultPrevented) {
      return;
    }

    e.preventDefault();
    if (!e.dataTransfer) {
      return;
    }

    // A type can be present with empty data, e.g. setData(type, ''). Fall
    // through to the other types then, like the native toolbar.
    if (e.dataTransfer.types.includes('text/uri-list') &&
        e.dataTransfer.getData('text/uri-list')) {
      this.browserProxy_.toolbarUIHandler.onToolbarDropUrl();
    } else if (e.dataTransfer.types.includes('Files')) {
      this.browserProxy_.toolbarUIHandler.onToolbarDropFile(
          {x: e.clientX, y: e.clientY});
    } else if (e.dataTransfer.types.includes('text/plain')) {
      const text = e.dataTransfer.getData('text/plain');
      if (text) {
        this.browserProxy_.toolbarUIHandler.onToolbarDropText(text);
      }
    }
  }

  protected keyboardNavigable(): boolean {
    return this.toolbarState_.keyboardNavigable;
  }

  protected locationBarChipsKeyboardNavigable(): boolean {
    // <if expr="not is_macosx">
    // On non-Mac, location bar chips are almost always tab-focusable
    // (except location icon isn't in some cases).
    return true;
    // </if>

    // <if expr="is_macosx">
    // On Mac, location bar chips follow the system tab navigability
    // settings (except the AIM button is always focusable).
    return this.toolbarState_.keyboardNavigable;
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
