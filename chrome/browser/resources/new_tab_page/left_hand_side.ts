// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';
import './lhs_ghost_loader.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ThreadsRailElement} from 'chrome://resources/cr_components/composebox/threads_rail.js';
import {LhsClientAction} from 'chrome://resources/cr_components/composebox/threads_rail.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import type {IframeElement} from './iframe.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';
import {recordBoolean, recordDuration, recordThreadsRailExpandedAction, ThreadsRailExpandedAction} from './metrics_utils.js';
import {WindowProxy} from './window_proxy.js';

const DEFAULT_EXPANDED_LHS_WIDTH_PX = 261;

type NtpLhsThemeKey = keyof NtpLhsThemePalette;

interface NtpLhsSetThemeMessage {
  type: NtpLhsMessageType.NTP_LHS_SET_THEME;
  theme: NtpLhsThemePalette;
  isDark?: boolean;
  iconSize?: number;
}

type ChromeToSearchLhsMessage =
    NtpLhsSetThemeMessage|{type: NtpLhsMessageType.NTP_LHS_FOCUS};

// Maps each `NtpLhsThemePalette` key to its corresponding
// `--color-new-tab-page-threads-rail-<suffix>` CSS variable from
// `chrome_color_id.h`.
const NTP_LHS_THEME_CSS_VARS: Record<NtpLhsThemeKey, string> = {
  'primaryFont': 'primary-font',
  'secondaryFont': 'secondary-font',
  'railBackground': 'background',
  'flyoutMenuBackground': 'flyout-menu-background',
  'surfaceContainer': 'surface-container',
  'surfaceContainerHigh': 'surface-container-high',
  'surfaceContainerHighest': 'surface-container-highest',
  'primary': 'primary',
  'tooltipContainer': 'tooltip-container',
  'hoverBackgroundColor': 'hover-background',
  'selectedBackgroundColor': 'selected-background',
  'loaderBaseColor': 'loader-base',
  'loaderHighlightColor': 'loader-highlight',
  'brandPrimaryFont': 'brand-primary-font',
  'brandSecondaryFont': 'brand-secondary-font',
  'menuOpenBackgroundColor': 'menu-open-background',
  'containerBackground': 'container-background',
  'dividerColor': 'divider',
  'scrimColor': 'scrim',
  'scrollbarThumbColor': 'scrollbar-thumb',
  'updateDotColor': 'update-dot',
};

export enum NtpLhsMessageType {
  NTP_LHS_READY = 'NTP_LHS_READY',
  NTP_LHS_CLOSE = 'NTP_LHS_CLOSE',
  NTP_LHS_NEW_THREAD = 'NTP_LHS_NEW_THREAD',
  NTP_LHS_NAVIGATE = 'NTP_LHS_NAVIGATE',
  NTP_LHS_SET_THEME = 'NTP_LHS_SET_THEME',
  NTP_LHS_FOCUS = 'NTP_LHS_FOCUS',
}

export interface NtpLhsReadyMessage {
  type: NtpLhsMessageType.NTP_LHS_READY;
  width?: number;
}

export interface NtpLhsCloseMessage {
  type: NtpLhsMessageType.NTP_LHS_CLOSE;
}

export interface NtpLhsNewThreadMessage {
  type: NtpLhsMessageType.NTP_LHS_NEW_THREAD;
}

export interface NtpLhsNavigateMessage {
  type: NtpLhsMessageType.NTP_LHS_NAVIGATE;
  url: string;
}

export type SearchToChromeLhsMessage = NtpLhsReadyMessage|NtpLhsCloseMessage|
    NtpLhsNewThreadMessage|NtpLhsNavigateMessage;

/**
 * Theme palette forwarded to the embedded Search AI Mode LHS iframe via
 * `NTP_LHS_SET_THEME`. Property names must stay in sync with Search's
 * `NtpLhsThemePalette` (`ntp_lhs_post_message.ts`).
 */
export interface NtpLhsThemePalette {
  primaryFont?: string;
  secondaryFont?: string;
  railBackground?: string;
  flyoutMenuBackground?: string;
  surfaceContainer?: string;
  surfaceContainerHigh?: string;
  surfaceContainerHighest?: string;
  primary?: string;
  tooltipContainer?: string;
  hoverBackgroundColor?: string;
  selectedBackgroundColor?: string;
  loaderBaseColor?: string;
  loaderHighlightColor?: string;
  brandPrimaryFont?: string;
  brandSecondaryFont?: string;
  menuOpenBackgroundColor?: string;
  containerBackground?: string;
  dividerColor?: string;
  scrimColor?: string;
  scrollbarThumbColor?: string;
  updateDotColor?: string;
}

// The primary view of the LHS. #folded and #expanded stay rendered once the
// iframe is ready so that they can cross-fade (see left_hand_side.css), so
// more than one view may be painted during a transition.
export enum LhsView {
  // The folded threads rail.
  FOLDED = 'folded',
  // The ghost loader shown while the expanded iframe loads.
  GHOST_LOADER = 'ghostLoader',
  // The expanded LHS iframe.
  EXPANDED = 'expanded',
}

export interface LeftHandSideElement {
  $: {
    folded: ThreadsRailElement,
  };
}

export class LeftHandSideElement extends CrLitElement {
  static get is() {
    return 'cr-left-hand-side';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expandedLhsUrl_: {type: String},
      expandedWidth_: {type: Number},
      isDark: {
        type: Boolean,
        reflect: true,
      },
      isExpanded_: {
        type: Boolean,
        reflect: true,
      },
      isExpandedReady_: {type: Boolean},
      preloadStartTime_: {type: Number},
      threadsRailPreloadDebounceMs_: {type: Number},
      view_: {
        type: String,
        reflect: true,
      },
    };
  }

  accessor isDark: boolean|undefined = undefined;
  protected accessor expandedLhsUrl_: string =
      loadTimeData.getString('expandedLhsUrl');
  protected accessor expandedWidth_: number = DEFAULT_EXPANDED_LHS_WIDTH_PX;
  protected accessor isExpanded_: boolean = false;
  protected accessor isExpandedReady_: boolean = false;
  // Time at which <ntp-iframe id="expanded"> was stamped into the DOM so that
  // it starts loading expandedLhsUrl_, or null while it is not stamped. Set
  // once ensureLazyLoaded() resolves on hover, focus, or expand.
  protected accessor preloadStartTime_: number|null = null;
  private accessor threadsRailPreloadDebounceMs_: number =
      loadTimeData.valueExists('threadsRailPreloadDebounceMs') ?
      loadTimeData.getInteger('threadsRailPreloadDebounceMs') :
      100;
  // The primary view, reflected as the `view_` attribute so that
  // left_hand_side.css can style each view. Derived in willUpdate().
  protected accessor view_: LhsView = LhsView.FOLDED;
  // Accessible name shared by the ghost loader region and the expanded LHS
  // iframe.
  protected readonly lhsLabel_: string =
      loadTimeData.getString('aimThreadsHistoryLabel');
  private eventTracker_: EventTracker = new EventTracker();
  // Width of the folded rail, measured once it has rendered.
  private collapsedWidth_: number = 0;
  private preloadTimerId_: number|null = null;
  private isFoldedHovered_: boolean = false;
  private isFoldedFocused_: boolean = false;
  private hoverStartTime_: number|null = null;
  // Longest completed hover/focus interval on the folded rail, or null if none
  // has completed yet. Together with the active interval, this is recorded on
  // first expand or on pagehide to tune threadsRailPreloadDebounceMs_.
  private longestHoverDurationMs_: number|null = null;
  // Whether the user has requested to expand the LHS at least once.
  private hasExpanded_: boolean = false;
  private ghostLoaderShownTime_: number|null = null;
  private lastHiddenTime_: number|null = null;
  private hadFocusBeforeUpdate_: boolean = false;
  private focusCloseOnExpand_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    if (!this.expandedLhsUrl_) {
      return;
    }
    this.eventTracker_.add(window, 'message', this.onMessage_);
    this.eventTracker_.add(
        ColorChangeUpdater.forDocument().eventTarget, COLOR_PROVIDER_CHANGED,
        this.postThemeColors_);
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilityChange_);
    this.eventTracker_.add(window, 'pagehide', this.onPageHide_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.cancelPreloadTimer_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    this.view_ = this.getVisibleView_();
    this.hadFocusBeforeUpdate_ = this.shadowRoot.activeElement !== null;
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.collapsedWidth_ = this.$.folded.offsetWidth;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('view_')) {
      if (this.view_ === LhsView.EXPANDED) {
        recordBoolean('NewTabPage.ThreadsRail.ExpandedShown', true);
      }
      if (this.view_ === LhsView.GHOST_LOADER) {
        this.ghostLoaderShownTime_ = WindowProxy.getInstance().now();
      } else if (this.ghostLoaderShownTime_ !== null) {
        // Record how long the ghost loader was shown, until the expanded
        // iframe replaced it or the LHS was collapsed.
        this.recordLatency_(
            'NewTabPage.ThreadsRail.GhostLoaderShownDuration',
            this.ghostLoaderShownTime_, WindowProxy.getInstance().now());
        this.ghostLoaderShownTime_ = null;
      }
    }
    if (changedPrivateProperties.has('isExpanded_') ||
        changedPrivateProperties.has('expandedWidth_') ||
        changedPrivateProperties.has('collapsedWidth_')) {
      this.setHostWidth_(
          this.isExpanded_ ? this.expandedWidth_ : this.collapsedWidth_);
    }
    if (changedPrivateProperties.has('view_') &&
        (changedPrivateProperties.get('view_') === LhsView.FOLDED ||
         this.hadFocusBeforeUpdate_)) {
      this.focusView_();
    }
  }

  // Returns the primary view to show. Every view in getHtml() is inert unless
  // it is the primary view. Opacity and visibility for the cross-fade between
  // views, including hiding the folded rail while the ghost loader replaces
  // it, are handled in left_hand_side.css.
  private getVisibleView_(): LhsView {
    if (!this.isExpanded_) {
      return LhsView.FOLDED;
    }
    return this.isExpandedReady_ ? LhsView.EXPANDED : LhsView.GHOST_LOADER;
  }

  private setHostWidth_(width: number) {
    this.style.setProperty(
        '--cr-left-hand-side-width', `${Math.round(width)}px`);
  }

  // Returns the expanded LHS iframe, or null while it is not in the DOM. It is
  // only stamped once the folded rail is hovered, focused, or expanded (see
  // getHtml()), so that the embedded page is not loaded until the user
  // interacts with the rail.
  private getExpandedIframe_(): IframeElement|null {
    return this.shadowRoot.querySelector<IframeElement>('#expanded');
  }

  private focusView_() {
    switch (this.view_) {
      case LhsView.FOLDED:
        this.$.folded.focusItemWithAction(LhsClientAction.EXPAND_RAIL);
        break;
      case LhsView.GHOST_LOADER: {
        const selector =
            this.focusCloseOnExpand_ ? 'ntp-lhs-ghost-loader' : '#ghostLoader';
        this.shadowRoot.querySelector<HTMLElement>(selector)?.focus();
        break;
      }
      case LhsView.EXPANDED: {
        const expanded = this.getExpandedIframe_();
        expanded?.focus();
        if (this.focusCloseOnExpand_) {
          const message: ChromeToSearchLhsMessage = {
            type: NtpLhsMessageType.NTP_LHS_FOCUS,
          };
          expanded?.postMessage(message);
        }
        break;
      }
      default:
        break;
    }
  }

  protected onFoldedPointerenter_() {
    this.isFoldedHovered_ = true;
    this.onFoldedInteractionStart_();
  }

  protected onFoldedPointerleave_() {
    this.isFoldedHovered_ = false;
    this.onFoldedInteractionEnd_();
  }

  protected onFoldedFocusin_() {
    this.isFoldedFocused_ = true;
    this.onFoldedInteractionStart_();
  }

  protected onFoldedFocusout_(e: FocusEvent) {
    // focusout bubbles from the rail's children, so moving focus between them
    // fires focusout then focusin. Ignore it so that the hover/focus interval
    // and preload timer are not interrupted while focus stays in the rail.
    const folded = e.currentTarget as HTMLElement;
    if (e.relatedTarget instanceof Node && folded.contains(e.relatedTarget)) {
      return;
    }
    this.isFoldedFocused_ = false;
    this.onFoldedInteractionEnd_();
  }

  // Starts a hover/focus interval on the folded rail, unless one is already
  // active, and schedules the preload.
  private onFoldedInteractionStart_() {
    if (this.hoverStartTime_ === null) {
      this.hoverStartTime_ = WindowProxy.getInstance().now();
    }
    this.schedulePreloadExpanded_();
  }

  // Ends the hover/focus interval and cancels the preload timer once the folded
  // rail is neither hovered nor focused.
  private onFoldedInteractionEnd_() {
    if (this.isFoldedHovered_ || this.isFoldedFocused_) {
      return;
    }
    this.endHoverInterval_();
    this.cancelPreloadTimer_();
  }

  // Folds the active hover/focus interval, if any, into
  // longestHoverDurationMs_.
  private endHoverInterval_() {
    this.longestHoverDurationMs_ = this.getLongestHoverDurationMs_();
    this.hoverStartTime_ = null;
  }

  // Returns the longest continuous hover/focus interval on the folded rail so
  // far, including the active one, or null if the rail was never hovered or
  // focused.
  private getLongestHoverDurationMs_(): number|null {
    if (this.hoverStartTime_ === null) {
      return this.longestHoverDurationMs_;
    }
    return Math.max(
        this.longestHoverDurationMs_ ?? 0,
        WindowProxy.getInstance().now() - this.hoverStartTime_);
  }

  protected async onFoldedClick_() {
    await this.expand_(false);
  }

  protected async onExpandRail_(e: CustomEvent<MouseEvent>) {
    // Check `sourceCapabilities` rather than `detail === 0` since Blink sets
    // `detail = 1` on accessibility actions (`kFromAccessibility`).
    await this.expand_(e.detail.sourceCapabilities === null);
  }

  // Lets the user close the rail from the ghost loader, e.g. when the expanded
  // LHS iframe does not load.
  protected onGhostLoaderCloseRail_() {
    this.isExpanded_ = false;
  }

  private schedulePreloadExpanded_() {
    if (this.preloadStartTime_ !== null) {
      return;
    }
    if (this.preloadTimerId_ !== null) {
      return;
    }
    if (this.threadsRailPreloadDebounceMs_ <= 0) {
      this.preloadExpanded_();
      return;
    }
    this.preloadTimerId_ = WindowProxy.getInstance().setTimeout(() => {
      this.preloadTimerId_ = null;
      this.preloadExpanded_();
    }, this.threadsRailPreloadDebounceMs_);
  }

  private cancelPreloadTimer_() {
    if (this.preloadTimerId_ !== null) {
      WindowProxy.getInstance().clearTimeout(this.preloadTimerId_);
      this.preloadTimerId_ = null;
    }
  }

  private async preloadExpanded_() {
    if (this.preloadStartTime_ !== null) {
      return;
    }
    // <ntp-iframe> is defined in the lazily loaded bundle, which may not have
    // been loaded yet if the folded rail is hovered or clicked right after the
    // NTP loads.
    await WindowProxy.getInstance().ensureLazyLoaded();
    // A concurrent call (e.g. a click while the hover preload is awaiting
    // above) may have stamped the iframe already.
    if (this.preloadStartTime_ === null) {
      this.preloadStartTime_ = WindowProxy.getInstance().now();
    }
  }

  private async expand_(focusCloseButton: boolean) {
    if (!this.hasExpanded_) {
      this.hasExpanded_ = true;
      // Record whether the expanded iframe was already preloaded and ready at
      // the first expand request, i.e. whether the ghost loader is skipped.
      recordBoolean(
          'NewTabPage.ThreadsRail.FirstExpandWasPreloaded',
          this.isExpandedReady_);
      const longestHoverDurationMs = this.getLongestHoverDurationMs_();
      if (longestHoverDurationMs !== null) {
        // Record the longest continuous hover/focus on the folded rail before
        // the first expand request, to tune threadsRailPreloadDebounceMs_.
        recordDuration(
            'NewTabPage.ThreadsRail.LongestFocusDurationBeforeExpand',
            longestHoverDurationMs);
      }
    }
    this.cancelPreloadTimer_();
    await this.preloadExpanded_();
    this.focusCloseOnExpand_ = focusCloseButton;
    this.isExpanded_ = true;
  }

  private onMessage_ = (event: MessageEvent) => {
    const expectedOrigin = new URL(this.expandedLhsUrl_).origin;
    if (event.origin !== expectedOrigin) {
      return;
    }

    const iframe =
        this.getExpandedIframe_()?.shadowRoot.querySelector('iframe');
    if (!iframe || event.source !== iframe.contentWindow) {
      return;
    }

    const data = event.data as SearchToChromeLhsMessage | null;
    if (typeof data !== 'object' || !data) {
      return;
    }

    switch (data.type) {
      case NtpLhsMessageType.NTP_LHS_READY: {
        // Search posts NTP_LHS_READY once on initial mount; guard with
        // !isExpandedReady_ to ensure at most one sample per NTP.
        if (!this.isExpandedReady_) {
          recordBoolean('NewTabPage.ThreadsRail.ExpandedIframeLoaded', true);
          // The iframe only exists once stamped, see getHtml().
          assert(this.preloadStartTime_ !== null);
          // Record time from the expanded iframe being stamped until
          // NTP_LHS_READY.
          this.recordLatency_(
              'NewTabPage.ThreadsRail.IframeLoadLatency',
              this.preloadStartTime_, WindowProxy.getInstance().now());
        }
        this.postThemeColors_();
        this.expandedWidth_ = typeof data.width === 'number' && data.width > 0 ?
            data.width :
            DEFAULT_EXPANDED_LHS_WIDTH_PX;
        this.isExpandedReady_ = true;
        break;
      }
      case NtpLhsMessageType.NTP_LHS_CLOSE: {
        if (this.isExpanded_) {
          recordThreadsRailExpandedAction(
              ThreadsRailExpandedAction.CLOSE_CLICKED);
        }
        this.isExpanded_ = false;
        break;
      }
      case NtpLhsMessageType.NTP_LHS_NEW_THREAD: {
        recordThreadsRailExpandedAction(
            ThreadsRailExpandedAction.NEW_THREAD_CLICKED);
        this.fire('open-composebox');
        break;
      }
      case NtpLhsMessageType.NTP_LHS_NAVIGATE: {
        let url: URL;
        try {
          url = new URL(data.url);
        } catch {
          return;
        }
        if (url.protocol !== 'http:' && url.protocol !== 'https:') {
          return;
        }
        recordThreadsRailExpandedAction(
            ThreadsRailExpandedAction.NAVIGATE_CLICKED);
        WindowProxy.getInstance().navigate(data.url);
        break;
      }
      default:
        break;
    }
  };

  // Tracks when the NTP was last hidden (e.g. the user switched tabs), so that
  // latency samples spanning background time can be dropped in
  // recordLatency_(). Also ends the active hover/focus interval and cancels a
  // pending preload, since the user no longer dwells on the folded rail while
  // the page is hidden.
  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'hidden') {
      this.lastHiddenTime_ = WindowProxy.getInstance().now();
      this.isFoldedHovered_ = false;
      this.isFoldedFocused_ = false;
      this.onFoldedInteractionEnd_();
    }
  };

  // Records the longest continuous hover/focus on the folded rail if the NTP
  // is closed or navigated away from without the LHS ever being expanded. Its
  // distribution against LongestFocusDurationBeforeExpand is used to tune
  // threadsRailPreloadDebounceMs_.
  private onPageHide_ = () => {
    if (this.hasExpanded_) {
      return;
    }
    const longestHoverDurationMs = this.getLongestHoverDurationMs_();
    if (longestHoverDurationMs !== null) {
      recordDuration(
          'NewTabPage.ThreadsRail.LongestFocusDurationWithoutExpand',
          longestHoverDurationMs);
    }
  };

  // Records a latency sample, unless the page is hidden now or was hidden at
  // any point since `startTime`. While hidden, the user is not waiting and
  // background throttling delays timers and rendering, so such samples would
  // inflate the latency.
  private recordLatency_(name: string, startTime: number, endTime: number) {
    if (document.visibilityState === 'hidden' ||
        (this.lastHiddenTime_ !== null && this.lastHiddenTime_ >= startTime)) {
      return;
    }
    recordDuration(name, endTime - startTime);
  }

  private postThemeColors_ = () => {
    const expanded = this.getExpandedIframe_();
    if (!expanded) {
      // Nothing to post to before the expanded iframe is stamped. The embedded
      // page asks for the theme with NTP_LHS_READY once it is loaded.
      return;
    }
    const computedStyle = getComputedStyle(this);
    const theme: NtpLhsThemePalette = {};
    for (const [key, cssVarSuffix] of Object.entries(NTP_LHS_THEME_CSS_VARS) as
         Array<[NtpLhsThemeKey, string]>) {
      theme[key] = computedStyle
                       .getPropertyValue(
                           `--color-new-tab-page-threads-rail-${cssVarSuffix}`)
                       .trim();
    }
    const iconSize = parseInt(
        getComputedStyle(this.$.folded)
            .getPropertyValue('--threads-rail-icon-size'),
        10);
    const message: ChromeToSearchLhsMessage = {
      type: NtpLhsMessageType.NTP_LHS_SET_THEME,
      theme,
      isDark: this.isDark,
      ...(iconSize > 0 ? {iconSize} : {}),
    };
    expanded.postMessage(message);
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-left-hand-side': LeftHandSideElement;
  }
}

customElements.define(LeftHandSideElement.is, LeftHandSideElement);
