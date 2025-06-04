// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './iframe.js';
import './doodle_share_dialog.js';

import {assert} from 'chrome://resources/js/assert.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {IframeElement} from './iframe.js';
import {getCss} from './logo.css.js';
import {getHtml} from './logo.html.js';
import type {Doodle, DoodleShareChannel, ImageDoodle, PageHandlerRemote, Theme} from './new_tab_page.mojom-webui.js';
import {DoodleImageType} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {$$} from './utils.js';
import {WindowProxy} from './window_proxy.js';

// Shows the Google logo or a doodle if available.
export class LogoElement extends CrLitElement {
  static get is() {
    return 'ntp-logo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * If true displays the Google logo single-colored.
       */
      singleColored: {
        reflect: true,
        type: Boolean,
      },

      /**
       * Used to determine if we should display a dark mode doodle.
       */
      theme: {type: Object},

      loaded_: {type: Boolean},
      doodle_: {type: Object},
      imageDoodle_: {type: Object},
      showLogo_: {type: Boolean},
      showDoodle_: {type: Boolean},

      doodleBoxed_: {
        reflect: true,
        type: Boolean,
      },

      imageUrl_: {type: String},
      showAnimation_: {type: Boolean},
      animationUrl_: {type: String},
      iframeUrl_: {type: String},
      duration_: {type: String},
      height_: {type: String},
      width_: {type: String},
      expanded_: {type: Boolean},
      showShareDialog_: {type: Boolean},
      imageDoodleTabIndex_: {type: Number},
    };
  }

  accessor singleColored: boolean = false;
  accessor theme: Theme|null = null;
  private accessor loaded_: boolean = false;
  protected accessor doodle_: Doodle|null = null;
  protected accessor imageDoodle_: ImageDoodle|null = null;
  protected accessor showLogo_: boolean = false;
  protected accessor showDoodle_: boolean = false;
  private accessor doodleBoxed_: boolean = false;
  protected accessor imageUrl_: string = '';
  protected accessor showAnimation_: boolean = false;
  protected accessor animationUrl_: string = '';
  protected accessor iframeUrl_: string = '';
  private accessor duration_: string = '';
  private accessor height_: string = '';
  private accessor width_: string = '';
  protected accessor expanded_: boolean = false;
  protected accessor showShareDialog_: boolean = false;
  protected accessor imageDoodleTabIndex_: number = -1;

  private eventTracker_: EventTracker = new EventTracker();
  private pageHandler_: PageHandlerRemote;
  private imageClickParams_: string|null = null;
  private interactionLogUrl_: Url|null = null;
  private shareId_: string|null = null;

  constructor() {
    performance.mark('logo-creation-start');
    super();

    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.pageHandler_.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
      this.loaded_ = true;
      if (this.doodle_ && this.doodle_.interactive) {
        this.width_ = `${this.doodle_.interactive.width}px`;
        this.height_ = `${this.doodle_.interactive.height}px`;
      }
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'message', ({data}: MessageEvent) => {
      if (data['cmd'] === 'resizeDoodle') {
        assert(data.duration);
        this.duration_ = data.duration;
        assert(data.height);
        this.height_ = data.height;
        assert(data.width);
        this.width_ = data.width;
        this.expanded_ = true;
      } else if (data['cmd'] === 'sendMode') {
        this.sendMode_();
      }
    });
    // Make sure the doodle gets the mode in case it has already requested it.
    this.sendMode_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.imageDoodle_ = this.computeImageDoodle_();
    this.imageUrl_ = this.computeImageUrl_();
    this.animationUrl_ = this.computeAnimationUrl_();
    this.showDoodle_ = this.computeShowDoodle_();
    this.iframeUrl_ = this.computeIframeUrl_();
    this.showLogo_ = this.computeShowLogo_();
    this.doodleBoxed_ = this.computeDoodleBoxed_();
    this.imageDoodleTabIndex_ = this.computeImageDoodleTabIndex_();
  }

  override firstUpdated() {
    performance.measure('logo-creation', 'logo-creation-start');
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('theme')) {
      this.sendMode_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('duration_') ||
        changedPrivateProperties.has('height_') ||
        changedPrivateProperties.has('width_')) {
      this.onDurationHeightWidthChange_();
    }

    if (changedPrivateProperties.has('imageDoodle_')) {
      this.onImageDoodleChange_();
    }
  }

  private onImageDoodleChange_() {
    if (this.imageDoodle_) {
      this.style.setProperty(
          '--ntp-logo-box-color',
          skColorToRgba(this.imageDoodle_.backgroundColor));
    } else {
      this.style.removeProperty('--ntp-logo-box-color');
    }
    // Stop the animation (if it is running) and reset logging params since
    // mode change constitutes a new doodle session.
    this.showAnimation_ = false;
    this.imageClickParams_ = null;
    this.interactionLogUrl_ = null;
    this.shareId_ = null;
  }

  private computeImageDoodle_(): ImageDoodle|null {
    return this.doodle_ && this.doodle_.image && this.theme &&
        (this.theme.isDark ? this.doodle_.image.dark :
                             this.doodle_.image.light) ||
        null;
  }

  private computeShowLogo_(): boolean {
    return !!this.loaded_ && !this.showDoodle_;
  }

  private computeShowDoodle_(): boolean {
    return !!this.imageDoodle_ ||
        /* We hide interactive doodles when offline. Otherwise, the iframe
           would show an ugly error page. */
        !!this.doodle_ && !!this.doodle_.interactive && window.navigator.onLine;
  }

  /**
   * @returns The NTP's background color or null if the NTP does not have
   * a single background color, e.g. when a background image is set.
   */
  private computeBackgroundColor_(): SkColor|null {
    if (!this.theme || !!this.theme.backgroundImage) {
      return null;
    }

    return this.theme.backgroundColor;
  }

  private computeDoodleBoxed_(): boolean {
    const backgroundColor = this.computeBackgroundColor_();
    return !backgroundColor ||
        !!this.imageDoodle_ &&
        this.imageDoodle_.backgroundColor.value !== backgroundColor.value;
  }

  /**
   * Called when a simple or animated doodle was clicked. Starts animation if
   * clicking preview image of animated doodle. Otherwise, opens
   * doodle-associated URL in new tab/window.
   */
  protected onImageClick_() {
    if ($$<HTMLElement>(this, '#imageDoodle')!.tabIndex < 0) {
      return;
    }
    if (this.isCtaImageShown_()) {
      this.showAnimation_ = true;
      this.pageHandler_.onDoodleImageClicked(
          DoodleImageType.kCta, this.interactionLogUrl_);

      // TODO(tiborg): This is technically not correct since we don't know if
      // the animation has loaded yet. However, since the animation is loaded
      // inside an iframe retrieving the proper load signal is not trivial. In
      // practice this should be good enough but we could improve that in the
      // future.
      this.logImageRendered_(
          DoodleImageType.kAnimation,
          this.imageDoodle_!.animationImpressionLogUrl!);

      if (!this.doodle_!.image!.onClickUrl) {
        $$<HTMLElement>(this, '#imageDoodle')!.blur();
      }

      return;
    }
    assert(this.doodle_!.image!.onClickUrl);
    this.pageHandler_.onDoodleImageClicked(
        this.showAnimation_ ? DoodleImageType.kAnimation :
                              DoodleImageType.kStatic,
        null);
    const onClickUrl = new URL(this.doodle_!.image!.onClickUrl.url);
    if (this.imageClickParams_) {
      for (const param of new URLSearchParams(this.imageClickParams_)) {
        onClickUrl.searchParams.append(param[0], param[1]);
      }
    }
    WindowProxy.getInstance().open(onClickUrl.toString());
  }

  protected onImageLoad_() {
    this.logImageRendered_(
        this.isCtaImageShown_() ? DoodleImageType.kCta :
                                  DoodleImageType.kStatic,
        this.imageDoodle_!.imageImpressionLogUrl);
  }

  private async logImageRendered_(type: DoodleImageType, logUrl: Url) {
    const {imageClickParams, interactionLogUrl, shareId} =
        await this.pageHandler_.onDoodleImageRendered(
            type, WindowProxy.getInstance().now(), logUrl);
    this.imageClickParams_ = imageClickParams;
    this.interactionLogUrl_ = interactionLogUrl;
    this.shareId_ = shareId;
  }

  protected onImageKeydown_(e: KeyboardEvent) {
    if ([' ', 'Enter'].includes(e.key)) {
      this.onImageClick_();
    }
  }

  protected onShare_(e: CustomEvent<DoodleShareChannel>) {
    const doodleId =
        new URL(this.doodle_!.image!.onClickUrl!.url).searchParams.get('ct');
    if (!doodleId) {
      return;
    }
    this.pageHandler_.onDoodleShared(e.detail, doodleId, this.shareId_);
  }

  private isCtaImageShown_(): boolean {
    return !this.showAnimation_ && !!this.imageDoodle_ &&
        !!this.imageDoodle_.animationUrl;
  }

  /**
   * Sends a postMessage to the interactive doodle whether the current theme is
   * dark or light. Won't do anything if we don't have an interactive doodle or
   * we haven't been told yet whether the current theme is dark or light.
   */
  private sendMode_() {
    if (!this.theme) {
      return;
    }
    const iframe = $$<IframeElement>(this, '#iframe');
    if (!iframe) {
      return;
    }
    iframe.postMessage({cmd: 'changeMode', dark: this.theme.isDark});
  }

  private computeImageUrl_(): string {
    return this.imageDoodle_ ? this.imageDoodle_.imageUrl.url : '';
  }

  private computeAnimationUrl_(): string {
    return this.imageDoodle_ && this.imageDoodle_.animationUrl ?
        `chrome-untrusted://new-tab-page/image?${
            this.imageDoodle_.animationUrl.url}` :
        '';
  }

  private computeIframeUrl_(): string {
    if (this.doodle_ && this.doodle_.interactive) {
      const url = new URL(this.doodle_.interactive.url.url);
      url.searchParams.append('theme_messages', '0');
      return url.href;
    } else {
      return '';
    }
  }

  protected onShareButtonClick_(e: Event) {
    e.stopPropagation();
    this.showShareDialog_ = true;
  }

  protected onShareDialogClose_() {
    this.showShareDialog_ = false;
  }

  private onDurationHeightWidthChange_() {
    this.duration_ ? this.style.setProperty('--duration', this.duration_) :
                     this.style.removeProperty('--duration');
    this.height_ ? this.style.setProperty('--height', this.height_) :
                   this.style.removeProperty('--height');
    this.width_ ? this.style.setProperty('--width', this.width_) :
                  this.style.removeProperty('--width');
  }

  private computeImageDoodleTabIndex_(): number {
    return (this.doodle_ && this.doodle_.image &&
            (this.isCtaImageShown_() || this.doodle_.image.onClickUrl)) ?
        0 :
        -1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-logo': LogoElement;
  }
}

customElements.define(LogoElement.is, LogoElement);
