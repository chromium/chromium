// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import './iframe.js';
import './doodle_share_dialog.js';

import {assert} from 'chrome://resources/js/assert.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from './i18n_setup.js';
import {IframeElement} from './iframe.js';
import {getTemplate} from './logo.html.js';
import {Doodle, DoodleImageType, DoodleShareChannel, ImageDoodle, PageHandlerRemote} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {$$} from './utils.js';
import {WindowProxy} from './window_proxy.js';

const SHARE_BUTTON_SIZE_PX: number = 26;

// Shows the Google logo or a doodle if available.
export class LogoElement extends PolymerElement {
  static get is() {
    return 'ntp-logo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If true displays the Google logo single-colored.
       */
      singleColored: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /**
       * If true displays the dark mode doodle if possible.
       */
      dark: {
        observer: 'onDarkChange_',
        type: Boolean,
      },

      /**
       * The NTP's background color. If null or undefined the NTP does not have
       * a single background color, e.g. when a background image is set.
       */
      backgroundColor: Object,

      loaded_: Boolean,

      doodle_: Object,

      imageDoodle_: {
        observer: 'onImageDoodleChange_',
        computed: 'computeImageDoodle_(dark, doodle_)',
        type: Object,
      },

      showLogo_: {
        computed: 'computeShowLogo_(loaded_, showDoodle_)',
        type: Boolean,
      },

      showDoodle_: {
        computed: 'computeShowDoodle_(doodle_, imageDoodle_)',
        type: Boolean,
      },

      doodleBoxed_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeDoodleBoxed_(backgroundColor, imageDoodle_)',
      },

      imageUrl_: {
        computed: 'computeImageUrl_(imageDoodle_)',
        type: String,
      },

      showAnimation_: {
        type: Boolean,
        value: false,
      },

      animationUrl_: {
        computed: 'computeAnimationUrl_(imageDoodle_)',
        type: String,
      },

      iframeUrl_: {
        computed: 'computeIframeUrl_(doodle_)',
        type: String,
      },

      duration_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      height_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      width_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      expanded_: Boolean,

      showShareDialog_: Boolean,

      imageDoodleTabIndex_: {
        type: Number,
        computed: 'computeImageDoodleTabIndex_(doodle_, showAnimation_)',
      },

      reducedLogoSpaceEnabled_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('reducedLogoSpaceEnabled'),
      },
    };
  }

  singleColored: boolean;
  dark: boolean;
  backgroundColor: SkColor;
  private loaded_: boolean;
  private doodle_: Doodle|null;
  private imageDoodle_: ImageDoodle|null;
  private showLogo_: boolean;
  private showDoodle_: boolean;
  private doodleBoxed_: boolean;
  private imageUrl_: string;
  private showAnimation_: boolean;
  private animationUrl_: string;
  private iframeUrl_: string;
  private duration_: string;
  private height_: string;
  private width_: string;
  private expanded_: boolean;
  private showShareDialog_: boolean;
  private imageDoodleTabIndex_: number;

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

  override ready() {
    super.ready();
    performance.measure('logo-creation', 'logo-creation-start');
  }

  private onImageDoodleChange_() {
    const shareButton = this.imageDoodle_ && this.imageDoodle_.shareButton;
    if (shareButton) {
      const height = this.imageDoodle_!.height;
      const width = this.imageDoodle_!.width;
      this.updateStyles({
        '--ntp-logo-share-button-background-color':
            skColorToRgba(shareButton.backgroundColor),
        '--ntp-logo-share-button-height':
            `${SHARE_BUTTON_SIZE_PX / height * 100}%`,
        '--ntp-logo-share-button-width':
            `${SHARE_BUTTON_SIZE_PX / width * 100}%`,
        '--ntp-logo-share-button-x': `${shareButton.x / width * 100}%`,
        '--ntp-logo-share-button-y': `${shareButton.y / height * 100}%`,
      });
    } else {
      this.updateStyles({
        '--ntp-logo-share-button-background-color': null,
        '--ntp-logo-share-button-height': null,
        '--ntp-logo-share-button-width': null,
        '--ntp-logo-share-button-x': null,
        '--ntp-logo-share-button-y': null,
      });
    }
    if (this.imageDoodle_) {
      this.updateStyles({
        '--ntp-logo-box-color':
            skColorToRgba(this.imageDoodle_.backgroundColor),
      });
    } else {
      this.updateStyles({
        '--ntp-logo-box-color': null,
      });
    }
    // Stop the animation (if it is running) and reset logging params since
    // mode change constitutes a new doodle session.
    this.showAnimation_ = false;
    this.imageClickParams_ = null;
    this.interactionLogUrl_ = null;
    this.shareId_ = null;
  }

  private computeImageDoodle_(): ImageDoodle|null {
    return this.doodle_ && this.doodle_.image &&
        (this.dark ? this.doodle_.image.dark : this.doodle_.image.light) ||
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

  private computeDoodleBoxed_(): boolean {
    return !this.backgroundColor ||
        !!this.imageDoodle_ &&
        this.imageDoodle_.backgroundColor.value !== this.backgroundColor.value;
  }

  /**
   * Called when a simple or animated doodle was clicked. Starts animation if
   * clicking preview image of animated doodle. Otherwise, opens
   * doodle-associated URL in new tab/window.
   */
  private onImageClick_() {
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
    const onClickUrl = new URL(this.doodle_!.image!.onClickUrl!.url);
    if (this.imageClickParams_) {
      for (const param of new URLSearchParams(this.imageClickParams_)) {
        onClickUrl.searchParams.append(param[0], param[1]);
      }
    }
    WindowProxy.getInstance().open(onClickUrl.toString());
  }

  private onImageLoad_() {
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

  private onImageKeydown_(e: KeyboardEvent) {
    if ([' ', 'Enter'].includes(e.key)) {
      this.onImageClick_();
    }
  }

  private onShare_(e: CustomEvent<DoodleShareChannel>) {
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
   * Sends a postMessage to the interactive doodle whether the  current theme is
   * dark or light. Won't do anything if we don't have an interactive doodle or
   * we haven't been told yet whether the current theme is dark or light.
   */
  private sendMode_() {
    const iframe = $$<IframeElement>(this, '#iframe');
    if (this.dark === undefined || !iframe) {
      return;
    }
    iframe.postMessage({cmd: 'changeMode', dark: this.dark});
  }

  private onDarkChange_() {
    this.sendMode_();
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

  private onShareButtonClick_(e: Event) {
    e.stopPropagation();
    this.showShareDialog_ = true;
  }

  private onShareDialogClose_() {
    this.showShareDialog_ = false;
  }

  private onDurationHeightWidthChange_() {
    this.updateStyles({
      '--duration': this.duration_,
      '--height': this.height_,
      '--width': this.width_,
    });
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
