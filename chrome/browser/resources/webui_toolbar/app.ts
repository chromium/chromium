// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './back_forward_button.js';
import './reload_button.js';
import './location_bar.js';
import './split_tabs_button.js';
import './home_button.js';
import './pinned_toolbar_actions.js';
import './avatar_button.js';
import './icon_table.js';
import './icon_from_table.js';
import './icons.html.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl, EventDispositionFlag, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, IconUpdate, NavigationControlsState, NavigationControlsStateListenerHandle} from './browser_proxy.js';
import {IconTable} from './icon_table.js';
import {MetricsRecorder} from './metrics_recorder.js';
import {AppMenuIconType, AppMenuSeverity} from './toolbar_ui_api_data_model.mojom-webui.js';
// clang-format off
// Helper so tests can find what they needed when optimization is on.
// This should probably be a separate file, but rollup support only
// handles 2 at most now.
import {
  ContentSettingImageType,
  IconType,
  LhsChipIdentifier,
  OmniboxTextColor,
  PermissionAction,
  PermissionChipTheme,
  PermissionPromptStyle,
  SplitTabActiveLocation,
} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {OmniboxAction, LocationBarState, PermissionChipState} from './toolbar_ui_api_data_model.mojom-webui.js';
import {INVALID_FOCUS_REQUEST_HANDLE} from './browser_proxy.js';
import {ContentSettingIconElement} from './content_setting_icon.js';
import {ContentSettingsIconsElement} from './content_settings_icons.js';
import type {IconFromTableElement} from './icon_from_table.js';
import {LocationBarElement} from './location_bar.js';
import {LocationIconElement} from './location_icon.js';
import {PointerProxyImpl} from './pointer_proxy.js';
import type {PointerProxy} from './pointer_proxy.js';
import {PermissionChipElement} from './permission_chip.js';
import {ReadonlyOmniboxElement} from './readonly_omnibox.js';
import {getClickSourceType, getContextMenuSourceType} from './toolbar_button.js';

export {
  BrowserProxyImpl,
  ContentSettingIconElement,
  ContentSettingImageType,
  ContentSettingsIconsElement,
  EventDispositionFlag,
  getClickSourceType,
  getContextMenuSourceType,
  IconTable,
  IconType,
  INVALID_FOCUS_REQUEST_HANDLE,
  INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE,
  LhsChipIdentifier,
  LocationBarElement,
  LocationIconElement,
  OmniboxTextColor,
  PermissionAction,
  PermissionChipElement,
  PermissionChipTheme,
  PermissionPromptStyle,
  PointerProxyImpl,
  ReadonlyOmniboxElement,
  TrackedElementManager,
};
export type {
  IconFromTableElement,
  LocationBarState,
  OmniboxAction,
  PermissionChipState,
  PointerProxy,
};
// clang-format on

const TRACKED_ELEMENTS: Array<{selector: string, id: string}> = [
  {selector: '#back', id: 'kToolbarBackButtonElementId'},
  {selector: '#forward', id: 'kToolbarForwardButtonElementId'},
  {selector: '#reload', id: 'kReloadButtonElementId'},
  {selector: '#split-tabs', id: 'kToolbarSplitTabsToolbarButtonElementId'},
  {selector: '#location-bar', id: 'kLocationBarElementId'},
  {selector: '#home', id: 'kToolbarHomeButtonElementId'},
  {selector: '#avatar', id: 'kToolbarAvatarButtonElementId'},
];

const AppElementBase = HelpBubbleMixinLit(CrLitElement);

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
      isSplitTabsButtonEnabled_: {type: Boolean},
      isHomeButtonEnabled_: {type: Boolean},
      isLocationBarEnabled_: {type: Boolean},
      navigationControlsState_: {type: Object},
      isBackForwardButtonEnabled_: {type: Boolean},
      isPinnedToolbarActionsEnabled_: {type: Boolean},
      isAvatarButtonEnabled_: {type: Boolean},
      isInitialized_: {type: Boolean},
    };
  }

  protected accessor isReloadButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableReloadButton');
  protected accessor isSplitTabsButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableSplitTabsButton');
  protected accessor isHomeButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableHomeButton');
  protected accessor isLocationBarEnabled_: boolean =
      loadTimeData.getBoolean('enableLocationBar');
  protected accessor isBackForwardButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableBackForwardButtons');
  protected accessor isPinnedToolbarActionsEnabled_: boolean =
      loadTimeData.getBoolean('enablePinnedToolbarActions');
  protected accessor isAvatarButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableAvatarButton');
  /**
   * Tracks whether the element has received its first navigation state
   * update from the browser and completed its initial visual render.
   */
  protected accessor isInitialized_: boolean =
      !loadTimeData.getBoolean('initialWebUISurfaceSyncEnabled');
  protected accessor navigationControlsState_: NavigationControlsState = {
    reloadControlState: {
      // While this will be overwritten anyways, this matches the default value
      // on some platforms.
      doubleClickInterval: {microseconds: BigInt(500 * 1000)},

      canShowMenu: false,
      isNavigationLoading: false,
      isContextMenuVisible: false,
      stateToken: 0,
    },
    splitTabsControlState: {
      isCurrentTabSplit: false,
      location: SplitTabActiveLocation.kStart,
      isPinned: false,
      isContextMenuVisible: false,
    },
    backForwardControlState: {
      backButtonState:
          {enabled: false, shouldBeShown: true, isContextMenuVisible: false},
      forwardButtonState:
          {enabled: false, shouldBeShown: true, isContextMenuVisible: false},
      backButtonLeadingMargin: 0,
    },
    homeControlState: {
      shouldBeShown: false,
      isContextMenuVisible: false,
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
    locationBarState: {
      omniboxViewState: {
        browserVersion: 0,
        uiVersion: 0,
        textPieces: [],
        inlineAutocompletion: '',
        additionalText: '',
        selection: null,
        textIsUrl: false,
      },
      locationBarFlags: {
        userInputInProgress: false,
        popupOpen: false,
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
    },
    avatarControlState: {
      iconUrl: '',
      text: '',
      tooltip: '',
      accessibilityName: '',
      accessibilityDescription: '',
    },
    layoutConstantsVersion: 0,
    pinnedToolbarActionsState: [],
  };

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;
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

  constructor() {
    super();
    this.addEventListener('contextmenu', e => {
      // Suppress the default browser context menu (which includes "Inspect") to
      // align with native toolbar behavior. Any elements that require a
      // custom context menu are responsible for triggering their own menus.
      e.preventDefault();
    });
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
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

    this.metricsRecorder_.startObserving();
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
        });
      }
    }

    const waitSelectors =
        ['#back', '#forward', '#reload', '#split-tabs', '#home', '#avatar'];
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
        !loadTimeData.getBoolean('initialWebUISurfaceSyncEnabled');
    this.initializeSessionId_++;

    this.metricsRecorder_.stopObserving();
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
         e.dataTransfer.types.includes('Files'))) {
      e.preventDefault();
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

    const url = e.dataTransfer.getData('text/uri-list');
    if (url) {
      this.browserProxy_.browserControlsHandler.navigate(
          url.split('\n')[0]!);
    } else if (e.dataTransfer.types.includes('Files')) {
      this.browserProxy_.toolbarUIHandler.onToolbarDropFile(
          {x: e.clientX, y: e.clientY});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
