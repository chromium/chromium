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

import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, NavigationControlsState, NavigationControlsStateListenerHandle} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';
import {SplitTabActiveLocation} from './toolbar_ui_api_data_model.mojom-webui.js';
// clang-format off
// Helper so tests can find what they needed when optimization is on.
// This should probably be a separate file, but rollup support only
// handles 2 at most now.
import {
  ContentSettingImageType,
  LhsChipIdentifier,
  OmniboxTextColor,
  SecurityChipIcon,
} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {OmniboxAction, LocationBarState} from './toolbar_ui_api_data_model.mojom-webui.js';
import {ReadonlyOmniboxElement} from './readonly_omnibox.js';
import {LocationBarElement} from './location_bar.js';
import {LocationIconElement} from './location_icon.js';
import {ContentSettingIconElement} from './content_setting_icon.js';
import {ContentSettingsIconsElement} from './content_settings_icons.js';

export {
  BrowserProxyImpl,
  ContentSettingIconElement,
  ContentSettingImageType,
  ContentSettingsIconsElement,
  LhsChipIdentifier,
  LocationBarElement,
  LocationIconElement,
  OmniboxTextColor,
  ReadonlyOmniboxElement,
  SecurityChipIcon,
  TrackedElementManager,
};
export type {
  LocationBarState,
  OmniboxAction,
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

  override render() {
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
    locationBarState: {
      omniboxViewState: {
        textPieces: [],
        inlineAutocompletion: '',
        selection: null,
        textIsUrl: false,
      },
      locationBarFlags: {
        userInputInProgress: false,
        popupOpen: false,
      },
      contentSettingImageStates: [],
      lhsChipsState: {
        securityChip: {
          icon: 0,
          securityLevel: 0,
          text: '',
          isClickable: false,
          isTextDangerous: false,
          isVisible: true,
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
    },
    layoutConstantsVersion: 0,
    pinnedToolbarActionsState: [],
  };

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;
  private trackedElementManager_: TrackedElementManager;
  private navigationStateListenerHandle_:
      NavigationControlsStateListenerHandle =
          INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;

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
    this.trackedElementManager_ = TrackedElementManager.getInstance();
    ColorChangeUpdater.forDocument().start();
  }

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

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
            (state: NavigationControlsState) => {
              this.navigationControlsState_ = state;
            });

    this.metricsRecorder_.startObserving();
    for (const {selector, id} of TRACKED_ELEMENTS) {
      const el = this.shadowRoot.querySelector<HTMLElement>(selector);
      if (el) {
        this.trackedElementManager_.startTracking(el, id, {
          onHighlightChanged: (highlighted: boolean) => {
            el.classList.toggle('anchor-highlight', highlighted);
          },
        });
        this.registerHelpBubble(id, el);
      }
    }
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.browserProxy_.removeNavigationStateListener(
        this.navigationStateListenerHandle_);

    this.metricsRecorder_.stopObserving();
    for (const {selector, id} of TRACKED_ELEMENTS) {
      const el = this.shadowRoot.querySelector<HTMLElement>(selector);
      if (el) {
        this.trackedElementManager_.stopTracking(el);
        this.unregisterHelpBubble(id);
      }
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

    const waitSelectors =
        ['#back', '#forward', '#reload', '#split-tabs', '#home', '#avatar'];
    const promises =
        waitSelectors.map(s => this.shadowRoot.querySelector<CrLitElement>(s))
            .filter(el => !!el)
            .map(el => el.updateComplete);

    Promise.all(promises).then(() => {
      this.browserProxy_.toolbarUIHandler.onPageInitialized();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
