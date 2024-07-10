// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * related info screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

import '//resources/ash/common/cr_elements/cr_lottie/cr_lottie.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../components/buttons/oobe_next_button.js';
import '../components/buttons/oobe_text_button.js';
import '../components/common_styles/oobe_dialog_host_styles.css.js';
import '../components/dialogs/oobe_adaptive_dialog.js';
import './assistant_common_styles.css.js';
import './assistant_icons.html.js';
import './setting_zippy.js';

import {afterNextRender, html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostMixin} from '../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../components/mixins/oobe_i18n_mixin.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {AssistantNativeIconType, webviewStripLinksContentScript} from './utils.js';


/**
 * Name of the screen.
 * @type {string}
 */
const RELATED_INFO_SCREEN_ID = 'RelatedInfoScreen';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const AssistantRelatedInfoBase =
    OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

/**
 * @polymer
 */
class AssistantRelatedInfo extends AssistantRelatedInfoBase {
  static get is() {
    return 'assistant-related-info';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether page content is loading.
       */
      loading: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether activity control consent is skipped.
       */
      skipActivityControl_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether to use same design for accept/decline buttons.
       */
      equalWeightButtons_: {
        type: Boolean,
        value: false,
      },

      /**
       * The given name of the user, if a child account is in use; otherwise,
       * this is an empty string.
       */
      childName_: {
        type: String,
        value: '',
      },

      /**
       * Whether the marketing opt-in page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * The animation URL template - loaded from loadTimeData.
     * The template is expected to have '$' instead of the locale.
     * @private {string}
     */
    this.urlTemplate_ =
        'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v5_omni_$.html';

    /**
     * Whether try to reload with the default url when a 404 error occurred.
     * @type {boolean}
     * @private
     */
    this.reloadWithDefaultUrl_ = false;

    /**
     * Whether an error occurs while the webview is loading.
     * @type {boolean}
     * @private
     */
    this.loadingError_ = false;

    /**
     * The animation webview object.
     * @type {Object}
     * @private
     */
    this.webview_ = null;

    /**
     * Whether the screen has been initialized.
     * @type {boolean}
     * @private
     */
    this.initialized_ = false;

    /**
     * Whether the response header has been received for the animation webview.
     * @type {boolean}
     * @private
     */
    this.headerReceived_ = false;

    /**
     * Whether the webview has been successfully loaded.
     * @type {boolean}
     * @private
     */
    this.webViewLoaded_ = false;

    /**
     * Whether all the consent text strings has been successfully loaded.
     * @type {boolean}
     * @private
     */
    this.consentStringLoaded_ = false;

    /**
     * Whether the screen has been shown to the user.
     * @type {boolean}
     * @private
     */
    this.screenShown_ = false;

    /** @private {?BrowserProxy} */
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  }

  setUrlTemplateForTesting(url) {
    this.urlTemplate_ = url;
  }

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['next-pressed']);
  }

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['skip-pressed']);
  }

  /**
   * Reloads the page.
   */
  reloadPage() {
    this.dispatchEvent(
        new CustomEvent('loading', {bubbles: true, composed: true}));
    this.loading = true;
    this.loadingError_ = false;
    this.headerReceived_ = false;
    const locale = this.locale.replace('-', '_').toLowerCase();
    this.webview_.src = this.urlTemplate_.replace('$', locale);
  }

  /**
   * Handles event when animation webview cannot be loaded.
   */
  onWebViewErrorOccurred(details) {
    if (details && details.error === 'net::ERR_ABORTED') {
      // Retry triggers net::ERR_ABORTED, so ignore it.
      // TODO(b/232592745): Replace with a state machine to handle aborts
      // gracefully and avoid duplicate reloads.
      return;
    }
    this.dispatchEvent(
        new CustomEvent('error', {bubbles: true, composed: true}));
    this.loadingError_ = true;
  }

  /**
   * Handles event when animation webview is loaded.
   */
  onWebViewContentLoad(details) {
    if (details == null) {
      return;
    }
    if (this.loadingError_ || !this.headerReceived_) {
      return;
    }
    if (this.reloadWithDefaultUrl_) {
      this.webview_.src = this.getDefaultAnimationUrl_();
      this.headerReceived_ = false;
      this.reloadWithDefaultUrl_ = false;
      return;
    }

    this.webViewLoaded_ = true;
    if (this.consentStringLoaded_) {
      this.onPageLoaded();
    }

    // The webview animation only starts playing when it is focused (in order
    // to make sure the animation and the caption are in sync).
    this.webview_.focus();
    setTimeout(() => {
      if (!this.equalWeightButtons_) {
        this.$['next-button'].focus();
      }
    }, 300);
  }

  /**
   * Handles event when webview request headers received.
   */
  onWebViewHeadersReceived(details) {
    if (details == null) {
      return;
    }
    this.headerReceived_ = true;
    if (details.statusCode === 404) {
      if (details.url !== this.getDefaultAnimationUrl_()) {
        this.reloadWithDefaultUrl_ = true;
        return;
      } else {
        this.onWebViewErrorOccurred();
      }
    } else if (details.statusCode !== 200) {
      this.onWebViewErrorOccurred();
    }
  }

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent(data) {
    this.skipActivityControl_ = !data['activityControlNeeded'];
    this.childName_ = data['childName'];
    if (!data['useNativeIcons']) {
      const url = this.isDarkModeActive_ ? 'info_outline_gm_grey500_24dp.png' :
                                           'info_outline_gm_grey600_24dp.png';
      this.$.zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(this.$.zippy.getWrappedIcon(
                  'https://www.gstatic.com/images/icons/material/system/2x/' +
                      url,
                  this.i18n('assistantScreenContextTitle'),
                  getComputedStyle(document.body)
                      .getPropertyValue('--cros-bg-color'))));
      this.$.zippy.nativeIconType = AssistantNativeIconType.NONE;
    } else {
      this.$.zippy.nativeIconType = AssistantNativeIconType.INFO;
    }
    this.equalWeightButtons_ = data['equalWeightButtons'];

    this.consentStringLoaded_ = true;
    if (this.webViewLoaded_) {
      this.onPageLoaded();
    }
  }

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    this.dispatchEvent(
        new CustomEvent('loaded', {bubbles: true, composed: true}));
    this.loading = false;
    if (!this.equalWeightButtons_) {
      this.$['next-button'].focus();
    }
    if (!this.hidden && !this.screenShown_) {
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  }

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    if (!this.initialized_) {
      this.webview_ = this.$['assistant-animation-webview'];
      this.initializeWebview_(this.webview_);
      this.reloadPage();
      this.initialized_ = true;
    } else {
      afterNextRender(this, () => {
        if (!this.equalWeightButtons_) {
          this.$['next-button'].focus();
        }
      });
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  }

  initializeWebview_(webview) {
    const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};
    webview.request.onErrorOccurred.addListener(
        this.onWebViewErrorOccurred.bind(this), requestFilter);
    webview.request.onHeadersReceived.addListener(
        this.onWebViewHeadersReceived.bind(this), requestFilter);
    webview.addEventListener(
        'contentload', this.onWebViewContentLoad.bind(this));
    webview.addContentScripts([webviewStripLinksContentScript]);
  }

  /**
   * Get default animation url for locale en.
   */
  getDefaultAnimationUrl_() {
    return this.urlTemplate_.replace('$', 'en_us');
  }

  /**
   * Returns the webview animation container.
   */
  getAnimationContainer() {
    return this.$['animation-container'];
  }

  /**
   * Returns the title of the dialog.
   */
  getDialogTitle_(locale, skipActivityControl, childName) {
    if (skipActivityControl) {
      return this.i18n('assistantRelatedInfoReturnedUserTitle');
    } else {
      return childName ?
          this.i18n('assistantRelatedInfoTitleForChild', childName) :
          this.i18n('assistantRelatedInfoTitle');
    }
  }
}

customElements.define(AssistantRelatedInfo.is, AssistantRelatedInfo);
