// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './iframe.js';
import './doodle_share_dialog.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {$$} from './utils.js';
import {WindowProxy} from './window_proxy.js';

/** @type {number} */
const SHARE_BUTTON_SIZE_PX = 26;

// Shows the Google logo or a doodle if available.
class LogoElement extends PolymerElement {
  static get is() {
    return 'ntp-logo';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If true displays the Google logo single-colored.
       * @type {boolean}
       */
      singleColored: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /**
       * If true displays the dark mode doodle if possible.
       * @type {boolean}
       */
      dark: {
        observer: 'onDarkChange_',
        type: Boolean,
      },

      /**
       * The NTP's background color. If null or undefined the NTP does not have
       * a single background color, e.g. when a background image is set.
       * @type {skia.mojom.SkColor}
       */
      backgroundColor: Object,

      /** @private */
      loaded_: Boolean,

      /** @private {newTabPage.mojom.Doodle} */
      doodle_: Object,

      /** @private {newTabPage.mojom.ImageDoodle} */
      imageDoodle_: {
        observer: 'onImageDoodleChange_',
        computed: 'computeImageDoodle_(dark, doodle_)',
        type: Object,
      },

      /** @private */
      showLogo_: {
        computed: 'computeShowLogo_(loaded_, showDoodle_)',
        type: Boolean,
      },

      /** @private */
      showDoodle_: {
        computed: 'computeShowDoodle_(doodle_, imageDoodle_)',
        type: Boolean,
      },

      /** @private */
      doodleBoxed_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeDoodleBoxed_(backgroundColor, imageDoodle_)',
      },

      /** @private */
      imageUrl_: {
        computed: 'computeImageUrl_(imageDoodle_)',
        type: String,
      },

      /** @private */
      showAnimation_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      animationUrl_: {
        computed: 'computeAnimationUrl_(imageDoodle_)',
        type: String,
      },

      /** @private */
      iframeUrl_: {
        computed: 'computeIframeUrl_(doodle_)',
        type: String,
      },

      /** @private */
      duration_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      /** @private */
      height_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      /** @private */
      width_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      /** @private */
      expanded_: Boolean,

      /** @private */
      showShareDialog_: Boolean,
    };
  }

  constructor() {
    performance.mark('logo-creation-start');
    super();
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.pageHandler_.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
      this.loaded_ = true;
      if (this.doodle_ && this.doodle_.interactive) {
        this.width_ = `${this.doodle_.interactive.width}px`;
        this.height_ = `${this.doodle_.interactive.height}px`;
      }
    });
    /** @private {?string} */
    this.imageClickParams_ = null;
    /** @private {url.mojom.Url} */
    this.interactionLogUrl_ = null;
    /** @private {?string} */
    this.shareId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'message', ({data}) => {
      if (data['cmd'] === 'resizeDoodle') {
        this.duration_ = assert(data.duration);
        this.height_ = assert(data.height);
        this.width_ = assert(data.width);
        this.expanded_ = true;
      } else if (data['cmd'] === 'sendMode') {
        this.sendMode_();
      }
    });
    // Make sure the doodle gets the mode in case it has already requested it.
    this.sendMode_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    performance.measure('logo-creation', 'logo-creation-start');
  }

  /** @private */
  onImageDoodleChange_() {
    const shareButton = this.imageDoodle_ && this.imageDoodle_.shareButton;
    if (shareButton) {
      const height = this.imageDoodle_.height;
      const width = this.imageDoodle_.width;
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

  /**
   * @return {newTabPage.mojom.ImageDoodle}
   * @private
   */
  computeImageDoodle_() {
    return this.doodle_ && this.doodle_.image &&
        (this.dark ? this.doodle_.image.dark : this.doodle_.image.light) ||
        null;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowLogo_() {
    return !!this.loaded_ && !this.showDoodle_;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowDoodle_() {
    return !!this.imageDoodle_ ||
        /* We hide interactive doodles when offline. Otherwise, the iframe
           would show an ugly error page. */
        !!this.doodle_ && !!this.doodle_.interactive && window.navigator.onLine;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeDoodleBoxed_() {
    return !this.backgroundColor ||
        !!this.imageDoodle_ &&
        this.imageDoodle_.backgroundColor.value !== this.backgroundColor.value;
  }

  /**
   * Called when a simple or animated doodle was clicked. Starts animation if
   * clicking preview image of animated doodle. Otherwise, opens
   * doodle-associated URL in new tab/window.
   * @private
   */
  onImageClick_() {
    if (this.isCtaImageShown_()) {
      this.showAnimation_ = true;
      this.pageHandler_.onDoodleImageClicked(
          newTabPage.mojom.DoodleImageType.kCta, this.interactionLogUrl_);

      // TODO(tiborg): This is technically not correct since we don't know if
      // the animation has loaded yet. However, since the animation is loaded
      // inside an iframe retrieving the proper load signal is not trivial. In
      // practice this should be good enough but we could improve that in the
      // future.
      this.logImageRendered_(
          newTabPage.mojom.DoodleImageType.kAnimation,
          /** @type {!url.mojom.Url} */
          (this.imageDoodle_.animationImpressionLogUrl));

      return;
    }
    this.pageHandler_.onDoodleImageClicked(
        this.showAnimation_ ? newTabPage.mojom.DoodleImageType.kAnimation :
                              newTabPage.mojom.DoodleImageType.kStatic,
        null);
    const onClickUrl = new URL(this.doodle_.image.onClickUrl.url);
    if (this.imageClickParams_) {
      for (const param of new URLSearchParams(this.imageClickParams_)) {
        onClickUrl.searchParams.append(param[0], param[1]);
      }
    }
    WindowProxy.getInstance().open(onClickUrl.toString());
  }

  /** @private */
  onImageLoad_() {
    this.logImageRendered_(
        this.isCtaImageShown_() ? newTabPage.mojom.DoodleImageType.kCta :
                                  newTabPage.mojom.DoodleImageType.kStatic,
        this.imageDoodle_.imageImpressionLogUrl);
  }

  /**
   * @param {newTabPage.mojom.DoodleImageType} type
   * @param {!url.mojom.Url} logUrl
   * @private
   */
  async logImageRendered_(type, logUrl) {
    const {imageClickParams, interactionLogUrl, shareId} =
        await this.pageHandler_.onDoodleImageRendered(
            type, WindowProxy.getInstance().now(), logUrl);
    this.imageClickParams_ = imageClickParams;
    this.interactionLogUrl_ = interactionLogUrl;
    this.shareId_ = shareId;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onImageKeydown_(e) {
    if ([' ', 'Enter'].includes(e.key)) {
      this.onImageClick_();
    }
  }

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onShare_(e) {
    const doodleId =
        new URL(this.doodle_.image.onClickUrl.url).searchParams.get('ct');
    if (!doodleId) {
      return;
    }
    this.pageHandler_.onDoodleShared(e.detail, doodleId, this.shareId_);
  }

  /**
   * @returns {boolean}
   * @private
   */
  isCtaImageShown_() {
    return !this.showAnimation_ && !!this.imageDoodle_.animationUrl;
  }

  /**
   * Sends a postMessage to the interactive doodle whether the  current theme is
   * dark or light. Won't do anything if we don't have an interactive doodle or
   * we haven't been told yet whether the current theme is dark or light.
   * @private
   */
  sendMode_() {
    const iframe = $$(this, '#iframe');
    if (this.dark === undefined || !iframe) {
      return;
    }
    iframe.postMessage({cmd: 'changeMode', dark: this.dark});
  }

  /** @private */
  onDarkChange_() {
    this.sendMode_();
  }

  /**
   * @return {string}
   * @private
   */
  computeImageUrl_() {
    return this.imageDoodle_ ? this.imageDoodle_.imageUrl.url : '';
  }

  /**
   * @return {string}
   * @private
   */
  computeAnimationUrl_() {
    return this.imageDoodle_ && this.imageDoodle_.animationUrl ?
        `chrome-untrusted://new-tab-page/image?${
            this.imageDoodle_.animationUrl.url}` :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  computeIframeUrl_() {
    if (this.doodle_ && this.doodle_.interactive) {
      const url = new URL(this.doodle_.interactive.url.url);
      url.searchParams.append('theme_messages', '0');
      return url.href;
    } else {
      return '';
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onShareButtonClick_(e) {
    e.stopPropagation();
    this.showShareDialog_ = true;
  }

  /** @private */
  onShareDialogClose_() {
    this.showShareDialog_ = false;
  }

  /** @private */
  onDurationHeightWidthChange_() {
    this.updateStyles({
      '--duration': this.duration_,
      '--height': this.height_,
      '--width': this.width_,
    });
  }
}

customElements.define(LogoElement.is, LogoElement);
