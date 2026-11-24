// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';
import './lhs_ghost_loader.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import type {IframeElement} from './iframe.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';
import {recordBoolean, recordEnumeration} from './metrics_utils.js';
import {WindowProxy} from './window_proxy.js';

const DEFAULT_EXPANDED_LHS_WIDTH_PX = 261;

// LINT.IfChange(NTPThreadsRailExpandedAction)
/**
 * User actions within the expanded threads rail forwarded to Chrome. This enum
 * must match the numbering for NTPThreadsRailExpandedAction in enums.xml. These
 * values are persisted to logs. Entries should not be renumbered, removed or
 * reused.
 */
export enum ThreadsRailExpandedAction {
  CLOSE_CLICKED = 0,
  NEW_THREAD_CLICKED = 1,
  NAVIGATE_CLICKED = 2,
  MAX_VALUE = NAVIGATE_CLICKED,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NTPThreadsRailExpandedAction)

function recordThreadsRailExpandedAction(action: ThreadsRailExpandedAction) {
  recordEnumeration(
      'NewTabPage.ThreadsRail.ExpandedAction', action,
      ThreadsRailExpandedAction.MAX_VALUE + 1);
}

type NtpLhsThemeKey = keyof NtpLhsThemePalette;

interface NtpLhsSetThemeMessage {
  type: NtpLhsMessageType.NTP_LHS_SET_THEME;
  theme: NtpLhsThemePalette;
  isDark?: boolean;
  iconSize?: number;
}

type ChromeToSearchLhsMessage = NtpLhsSetThemeMessage;

const NTP_LHS_THEME_CSS_VARS: Record<NtpLhsThemeKey, string> = {
  'primaryFont': 'primary-font',
  'railBackground': 'background',
};

export enum NtpLhsMessageType {
  NTP_LHS_READY = 'NTP_LHS_READY',
  NTP_LHS_CLOSE = 'NTP_LHS_CLOSE',
  NTP_LHS_NEW_THREAD = 'NTP_LHS_NEW_THREAD',
  NTP_LHS_NAVIGATE = 'NTP_LHS_NAVIGATE',
  NTP_LHS_SET_THEME = 'NTP_LHS_SET_THEME',
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

export interface NtpLhsThemePalette {
  primaryFont?: string;
  railBackground?: string;
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
      hasPreloaded_: {type: Boolean},
      isDark: {
        type: Boolean,
        reflect: true,
      },
      isExpanded_: {
        type: Boolean,
        reflect: true,
      },
      isExpandedReady_: {type: Boolean},
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
  protected accessor hasPreloaded_: boolean = false;
  protected accessor isExpanded_: boolean = false;
  protected accessor isExpandedReady_: boolean = false;
  protected accessor threadsRailPreloadDebounceMs_: number =
      loadTimeData.valueExists('threadsRailPreloadDebounceMs') ?
      loadTimeData.getInteger('threadsRailPreloadDebounceMs') :
      100;
  // The primary view, reflected as the `view_` attribute so that
  // left_hand_side.css can style each view. Derived in willUpdate().
  protected accessor view_: LhsView = LhsView.FOLDED;
  // Accessible name of the ghost loader. Together with aria-busy, it keeps the
  // LHS region in the a11y tree as busy while the expanded iframe loads,
  // instead of the region silently going missing.
  protected readonly ghostLoaderLabel_: string =
      loadTimeData.getString('aimThreadsHistoryLabel');
  private eventTracker_: EventTracker = new EventTracker();
  // Width of the folded rail, measured once it has rendered.
  private collapsedWidth_: number = 0;
  private preloadTimerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    if (!this.expandedLhsUrl_) {
      return;
    }
    this.eventTracker_.add(window, 'message', this.onMessage_);
    this.eventTracker_.add(
        ColorChangeUpdater.forDocument().eventTarget, COLOR_PROVIDER_CHANGED,
        this.postThemeColors_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.cancelPreloadTimer_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    this.view_ = this.getVisibleView_();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const folded = this.shadowRoot.querySelector<HTMLElement>('#folded');
    this.collapsedWidth_ = folded ? folded.offsetWidth : 0;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('view_') &&
        this.view_ === LhsView.EXPANDED) {
      recordBoolean('NewTabPage.ThreadsRail.ExpandedShown', true);
    }
    if (changedPrivateProperties.has('isExpanded_') ||
        changedPrivateProperties.has('expandedWidth_') ||
        changedPrivateProperties.has('collapsedWidth_')) {
      this.setHostWidth_(
          this.isExpanded_ ? this.expandedWidth_ : this.collapsedWidth_);
    }
  }

  // Returns the primary view to show. Every view in getHtml() is inert unless
  // it is the primary view. Opacity and visibility for the cross-fade between
  // views, including hiding the folded rail while the ghost loader replaces
  // it, are handled in left_hand_side.css.
  protected getVisibleView_(): LhsView {
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

  protected onFoldedPointerenter_() {
    this.schedulePreloadExpanded_();
  }

  protected onFoldedPointerleave_() {
    this.cancelPreloadTimer_();
  }

  protected onFoldedFocusin_() {
    this.schedulePreloadExpanded_();
  }

  protected onFoldedFocusout_() {
    this.cancelPreloadTimer_();
  }

  protected async onFoldedClick_() {
    await this.expand_();
  }

  protected async onExpandRail_() {
    await this.expand_();
  }

  private schedulePreloadExpanded_() {
    if (this.hasPreloaded_) {
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
    if (this.hasPreloaded_) {
      return;
    }
    // <ntp-iframe> is defined in the lazily loaded bundle, which may not have
    // been loaded yet if the folded rail is hovered or clicked right after the
    // NTP loads.
    await WindowProxy.getInstance().ensureLazyLoaded();
    this.hasPreloaded_ = true;
  }

  private async expand_() {
    this.cancelPreloadTimer_();
    await this.preloadExpanded_();
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
    const folded = this.shadowRoot.querySelector<HTMLElement>('#folded');
    assert(folded);
    const iconSize = parseInt(
        getComputedStyle(folded).getPropertyValue('--threads-rail-icon-size'),
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
