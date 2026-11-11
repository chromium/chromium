// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import type {IframeElement} from './iframe.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';
import {WindowProxy} from './window_proxy.js';

const DEFAULT_EXPANDED_LHS_WIDTH_PX = 261;

type NtpLhsThemeKey = keyof NtpLhsThemePalette;

interface NtpLhsSetThemeMessage {
  type: NtpLhsMessageType.NTP_LHS_SET_THEME;
  theme: NtpLhsThemePalette;
  isDark?: boolean;
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
      enableThreadsRailHybrid_: {
        type: Boolean,
        reflect: true,
      },
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
      isExpandedReady_: {
        type: Boolean,
        reflect: true,
      },
      threadsRailPreloadDebounceMs_: {type: Number},
    };
  }

  accessor isDark: boolean|undefined = undefined;
  protected accessor enableThreadsRailHybrid_: boolean =
      loadTimeData.getBoolean('enableThreadsRailHybrid');
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
  private eventTracker_: EventTracker = new EventTracker();
  private iframeResizeObserver_: ResizeObserver|null = null;
  private lastIframeWidth_: number = 0;
  // Width of the folded rail in hybrid mode, measured once it has rendered.
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
    this.iframeResizeObserver_?.disconnect();
    this.iframeResizeObserver_ = null;
    this.cancelPreloadTimer_();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    if (this.enableThreadsRailHybrid_) {
      const folded = this.shadowRoot.querySelector<HTMLElement>('#folded');
      this.collapsedWidth_ = folded ? folded.offsetWidth : 0;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (this.enableThreadsRailHybrid_ &&
        (changedPrivateProperties.has('isExpanded_') ||
         changedPrivateProperties.has('isExpandedReady_') ||
         changedPrivateProperties.has('expandedWidth_') ||
         changedPrivateProperties.has('collapsedWidth_'))) {
      this.setHostWidth_(
          (this.isExpanded_ && this.isExpandedReady_) ? this.expandedWidth_ :
                                                        this.collapsedWidth_);
    }
  }

  private setHostWidth_(width: number) {
    this.style.setProperty(
        '--cr-left-hand-side-width', `${Math.round(width)}px`);
  }

  // In full-iframe mode the embedded page owns the rail width and reports it
  // through `frame-sizing: content-width`. That width lands in one step, so the
  // NTP layout would jump while the rail animates. Mirror the iframe width onto
  // the host, which transitions it, so the rest of the NTP slides in sync.
  private observeIframeWidth_(iframe: HTMLIFrameElement) {
    if (this.iframeResizeObserver_) {
      return;
    }
    this.iframeResizeObserver_ = new ResizeObserver(entries => {
      this.onIframeResize_(entries[entries.length - 1]!);
    });
    this.iframeResizeObserver_.observe(iframe);
  }

  private onIframeResize_(entry: ResizeObserverEntry) {
    const width = Math.round(entry.contentRect.width);
    // The iframe is 0px wide until it is ready. Leave the host width alone so
    // the first non-zero width is applied without a transition. The observer
    // also fires for height-only resizes (e.g. when the browser window is
    // resized); ignore those so the expanded state inferred below is not
    // flipped by a width that did not change.
    if (width <= 0 || width === this.lastIframeWidth_) {
      return;
    }
    if (this.lastIframeWidth_ > 0) {
      // The embedded page owns the expanded state in full-iframe mode. Infer
      // it from the direction of the resize to pick the matching transition.
      this.isExpanded_ = width > this.lastIframeWidth_;
    }
    this.lastIframeWidth_ = width;
    this.setHostWidth_(width);
  }

  // Returns the expanded LHS iframe, or null while it is not in the DOM. In
  // hybrid mode it is only stamped once the folded rail is hovered, focused,
  // or expanded (see getHtml()), so that the embedded page is not loaded until
  // the user interacts with the rail.
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
        this.postThemeColors_();
        this.expandedWidth_ = typeof data.width === 'number' && data.width > 0 ?
            data.width :
            DEFAULT_EXPANDED_LHS_WIDTH_PX;
        this.isExpandedReady_ = true;
        if (!this.enableThreadsRailHybrid_) {
          this.observeIframeWidth_(iframe);
        }
        break;
      }
      case NtpLhsMessageType.NTP_LHS_CLOSE: {
        this.isExpanded_ = false;
        break;
      }
      case NtpLhsMessageType.NTP_LHS_NEW_THREAD: {
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
      // Nothing to post to before the rail is expanded in hybrid mode. The
      // embedded page asks for the theme with NTP_LHS_READY once it is loaded.
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
    const message: ChromeToSearchLhsMessage = {
      type: NtpLhsMessageType.NTP_LHS_SET_THEME,
      theme,
      isDark: this.isDark,
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
