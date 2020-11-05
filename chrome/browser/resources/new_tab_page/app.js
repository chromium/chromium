// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './middle_slot_promo.js';
import './most_visited.js';
import './customize_dialog.js';
import './voice_search_overlay.js';
import './iframe.js';
import './fakebox.js';
import './realbox.js';
import './logo.js';
import './modules/module_wrapper.js';
import './modules/modules.js'; // Registers module descriptors.
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BackgroundManager} from './background_manager.js';
import {BrowserProxy} from './browser_proxy.js';
import {BackgroundSelection, BackgroundSelectionType} from './customize_dialog.js';
import {ModuleDescriptor} from './modules/module_descriptor.js';
import {ModuleRegistry} from './modules/module_registry.js';
import {oneGoogleBarApi} from './one_google_bar_api.js';
import {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
import {$$} from './utils.js';

/**
 * @typedef {{
 *   commandId: promoBrowserCommand.mojom.Command<number>,
 *   clickInfo: !promoBrowserCommand.mojom.ClickInfo
 * }}
 */
let CommandData;

class AppElement extends PolymerElement {
  static get is() {
    return 'ntp-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      iframeOneGoogleBarEnabled_: {
        type: Boolean,
        value: () => {
          const params = new URLSearchParams(window.location.search);
          if (params.has('ogbinline')) {
            return false;
          }
          return loadTimeData.getBoolean('iframeOneGoogleBarEnabled') ||
              params.has('ogbiframe');
        },
        reflectToAttribute: true,
      },

      /** @private */
      oneGoogleBarModalOverlaysEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('oneGoogleBarModalOverlaysEnabled'),
      },

      /** @private */
      oneGoogleBarIframePath_: {
        type: String,
        value: () => {
          const params = new URLSearchParams();
          params.set(
              'paramsencoded',
              btoa(window.location.search.replace(/^[?]/, '&')));
          return `chrome-untrusted://new-tab-page/one-google-bar?${params}`;
        },
      },

      /** @private */
      oneGoogleBarLoaded_: {
        observer: 'oneGoogleBarLoadedChange_',
        type: Boolean,
        value: false,
      },

      /** @private */
      oneGoogleBarDarkThemeEnabled_: {
        type: Boolean,
        computed: `computeOneGoogleBarDarkThemeEnabled_(oneGoogleBarLoaded_,
            theme_, backgroundSelection_)`,
        observer: 'onOneGoogleBarDarkThemeEnabledChange_',
      },

      /** @private */
      showIframedOneGoogleBar_: {
        type: Boolean,
        value: false,
        computed: `computeShowIframedOneGoogleBar_(iframeOneGoogleBarEnabled_,
            lazyRender_)`,
      },

      /** @private {!newTabPage.mojom.Theme} */
      theme_: {
        observer: 'onThemeChange_',
        type: Object,
      },

      /** @private */
      showCustomizeDialog_: Boolean,

      /** @private */
      showVoiceSearchOverlay_: Boolean,

      /** @private */
      showBackgroundImage_: {
        computed: 'computeShowBackgroundImage_(theme_, backgroundSelection_)',
        observer: 'onShowBackgroundImageChange_',
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private {!BackgroundSelection} */
      backgroundSelection_: {
        type: Object,
        value: () => ({type: BackgroundSelectionType.NO_SELECTION}),
        observer: 'updateBackgroundImagePath_',
      },

      /** @private */
      backgroundImageAttribution1_: {
        type: String,
        computed: `computeBackgroundImageAttribution1_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttribution2_: {
        type: String,
        computed: `computeBackgroundImageAttribution2_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttributionUrl_: {
        type: String,
        computed: `computeBackgroundImageAttributionUrl_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      doodleAllowed_: {
        computed: 'computeDoodleAllowed_(showBackgroundImage_, theme_)',
        type: Boolean,
      },

      /** @private {skia.mojom.SkColor} */
      backgroundColor_: {
        computed: 'computeBackgroundColor_(showBackgroundImage_, theme_)',
        type: Object,
      },

      /** @private */
      logoColor_: {
        type: String,
        computed: 'computeLogoColor_(theme_, backgroundSelection_)',
      },

      /** @private */
      singleColoredLogo_: {
        computed: 'computeSingleColoredLogo_(theme_, backgroundSelection_)',
        type: Boolean,
      },

      /** @private */
      realboxEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxEnabled'),
      },

      /** @private */
      realboxShown_: {
        type: Boolean,
        computed: 'computeRealboxShown_(theme_)',
      },

      /** @private */
      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
        reflectToAttribute: true,
      },

      /** @private */
      modulesVisible_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private */
      middleSlotPromoLoaded_: Boolean,

      /** @private */
      modulesLoaded_: Boolean,

      /**
       * In order to avoid flicker, the promo and modules are hidden until both
       * are loaded. If modules are disabled, the promo is shown as soon as it
       * is loaded.
       * @private
       */
      promoAndModulesLoaded_: {
        type: Boolean,
        computed: `computePromoAndModulesLoaded_(middleSlotPromoLoaded_,
            modulesLoaded_)`,
        reflectToAttribute: true,
      },

      /** @private */
      modulesLoadedAndVisible_: {
        type: Boolean,
        computed: `computeModulesLoadedAndVisible_(promoAndModulesLoaded_,
            modulesVisible_)`,
        observer: 'onModulesLoadedAndVisibleChange_',
      },

      /**
       * If true, renders additional elements that were not deemed crucial to
       * to show up immediately on load.
       * @private
       */
      lazyRender_: Boolean,

      /** @private {!Array<!ModuleDescriptor>} */
      moduleDescriptors_: Object,

      /**
       * The <ntp-module-wrapper> element of the last dismissed module.
       * @type {?Element}
       * @private
       */
      dismissedModuleWrapper_: {
        type: Object,
        value: null,
      },

      /**
       * The message shown in the toast when a module is dismissed.
       * @type {string}
       * @private
       */
      dismissModuleToastMessage_: String,
    };
  }

  constructor() {
    performance.mark('app-creation-start');
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!BackgroundManager} */
    this.backgroundManager_ = BackgroundManager.getInstance();
    /** @private {?number} */
    this.setThemeListenerId_ = null;
    /** @private {?number} */
    this.setModulesVisibleListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    this.loadOneGoogleBar_();
    /** @private {boolean} */
    this.shouldPrintPerformance_ =
        new URLSearchParams(location.search).has('print_perf');
    /**
     * Initialized with the start of the performance timeline in case the
     * background image load is not triggered by JS.
     * @private {number}
     */
    this.backgroundImageLoadStartEpoch_ = performance.timeOrigin;
    /** @private {number} */
    this.backgroundImageLoadStart_ = 0;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          performance.measure('theme-set');
          this.theme_ = theme;
        });
    this.setModulesVisibleListenerId_ =
        this.callbackRouter_.setModulesVisible.addListener(visible => {
          this.modulesVisible_ = visible;
        });
    this.pageHandler_.updateModulesVisible();
    this.eventTracker_.add(window, 'message', (event) => {
      /** @type {!Object} */
      const data = event.data;
      // Something in OneGoogleBar is sending a message that is received here.
      // Need to ignore it.
      if (typeof data !== 'object') {
        return;
      }
      if ('frameType' in data && data.frameType === 'one-google-bar') {
        this.handleOneGoogleBarMessage_(event);
      }
    });
    this.eventTracker_.add(window, 'keydown', e => this.onWindowKeydown_(e));
    if (this.shouldPrintPerformance_) {
      // It is possible that the background image has already loaded by now.
      // If it has, we request it to re-send the load time so that we can
      // actually catch the load time.
      this.backgroundManager_.getBackgroundImageLoadTime().then(
          time => {
            const duration = time - this.backgroundImageLoadStartEpoch_;
            this.printPerformanceDatum_(
                'background-image-load', this.backgroundImageLoadStart_,
                duration);
            this.printPerformanceDatum_(
                'background-image-loaded',
                this.backgroundImageLoadStart_ + duration);
          },
          () => {
            console.error('Failed to capture background image load time');
          });
    }
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    // Let the browser breath and then render remaining elements.
    BrowserProxy.getInstance().waitForLazyRender().then(() => {
      this.lazyRender_ = true;
    });
    this.printPerformance_();
    performance.measure('app-creation', 'app-creation-start');
  }

  /**
   * @return {boolean}
   * @private
   */
  computeOneGoogleBarDarkThemeEnabled_() {
    if (!this.theme_ || !this.oneGoogleBarLoaded_) {
      return false;
    }
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_.isDark;
    }
  }

  /**
   * @return {!Promise}
   * @private
   */
  async loadOneGoogleBar_() {
    if (this.iframeOneGoogleBarEnabled_) {
      const oneGoogleBar = document.querySelector('#oneGoogleBar');
      if (oneGoogleBar) {
        oneGoogleBar.remove();
      }
      return;
    }

    const {parts} = await this.pageHandler_.getOneGoogleBarParts(
        window.location.search.replace(/^[?]/, '&'));
    if (!parts) {
      return;
    }

    const inHeadStyle = document.createElement('style');
    inHeadStyle.type = 'text/css';
    inHeadStyle.appendChild(document.createTextNode(parts.inHeadStyle));
    document.head.appendChild(inHeadStyle);

    const inHeadScript = document.createElement('script');
    inHeadScript.type = 'text/javascript';
    inHeadScript.appendChild(document.createTextNode(parts.inHeadScript));
    document.head.appendChild(inHeadScript);

    this.oneGoogleBarLoaded_ = true;
    const oneGoogleBar = document.querySelector('#oneGoogleBar');
    oneGoogleBar.innerHTML = parts.barHtml;

    const afterBarScript = document.createElement('script');
    afterBarScript.type = 'text/javascript';
    afterBarScript.appendChild(document.createTextNode(parts.afterBarScript));
    oneGoogleBar.parentNode.insertBefore(
        afterBarScript, oneGoogleBar.nextSibling);

    document.querySelector('#oneGoogleBarEndOfBody').innerHTML =
        parts.endOfBodyHtml;

    const endOfBodyScript = document.createElement('script');
    endOfBodyScript.type = 'text/javascript';
    endOfBodyScript.appendChild(document.createTextNode(parts.endOfBodyScript));
    document.body.appendChild(endOfBodyScript);

    this.pageHandler_.onOneGoogleBarRendered(BrowserProxy.getInstance().now());
    oneGoogleBarApi.trackDarkModeChanges();
  }

  /** @private */
  onOneGoogleBarDarkThemeEnabledChange_() {
    if (!this.oneGoogleBarLoaded_) {
      return;
    }
    if (this.iframeOneGoogleBarEnabled_) {
      $$(this, '#oneGoogleBar').postMessage({
        type: 'enableDarkTheme',
        enabled: this.oneGoogleBarDarkThemeEnabled_,
      });
      return;
    }
    oneGoogleBarApi.setForegroundLight(this.oneGoogleBarDarkThemeEnabled_);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowIframedOneGoogleBar_() {
    return this.iframeOneGoogleBarEnabled_ && this.lazyRender_;
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution1_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution1 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution1;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution2_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution2 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution2;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttributionUrl_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttributionUrl ?
            this.theme_.backgroundImageAttributionUrl.url :
            '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attributionUrl.url;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeRealboxShown_() {
    // If realbox is to match the Omnibox's theme, keep it hidden until the
    // theme arrives. Otherwise mismatching colors will cause flicker.
    return !loadTimeData.getBoolean('realboxMatchOmniboxTheme') ||
        !!this.theme_;
  }

  /**
   * @return {boolean}
   * @private
   */
  computePromoAndModulesLoaded_() {
    return this.middleSlotPromoLoaded_ &&
        (!loadTimeData.getBoolean('modulesEnabled') || this.modulesLoaded_);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeModulesLoadedAndVisible_() {
    return this.promoAndModulesLoaded_ && this.modulesVisible_;
  }

  /** @private */
  async onLazyRendered_() {
    if (!loadTimeData.getBoolean('modulesEnabled')) {
      return;
    }
    this.moduleDescriptors_ =
        await ModuleRegistry.getInstance().initializeModules();
  }

  /** @private */
  onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    this.pageHandler_.onVoiceSearchAction(
        newTabPage.mojom.VoiceSearchAction.kActivateSearchBox);
  }

  /** @private */
  onCustomizeClick_() {
    this.showCustomizeDialog_ = true;
  }

  /** @private */
  onCustomizeDialogClose_() {
    this.showCustomizeDialog_ = false;
  }

  /** @private */
  onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * Handles <CTRL> + <SHIFT> + <.> (also <CMD> + <SHIFT> + <.> on mac) to open
   * voice search.
   * @param {KeyboardEvent} e
   * @private
   */
  onWindowKeydown_(e) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.code === 'Period' && e.shiftKey) {
      this.showVoiceSearchOverlay_ = true;
      this.pageHandler_.onVoiceSearchAction(
          newTabPage.mojom.VoiceSearchAction.kActivateKeyboard);
    }
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoDismissModuleButtonClick_();
    }
  }

  /**
   * @param {skia.mojom.SkColor} skColor
   * @return {string}
   * @private
   */
  rgbaOrInherit_(skColor) {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowBackgroundImage_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return !!this.theme_ && !!this.theme_.backgroundImage;
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return false;
    }
  }

  /** @private */
  onShowBackgroundImageChange_() {
    this.backgroundManager_.setShowBackgroundImage(this.showBackgroundImage_);
  }

  /** @private */
  onThemeChange_() {
    if (this.theme_) {
      this.backgroundManager_.setBackgroundColor(this.theme_.backgroundColor);
    }
    this.updateBackgroundImagePath_();
  }

  /** @private */
  onModulesLoadedAndVisibleChange_() {
    if (this.modulesLoadedAndVisible_) {
      this.pageHandler_.onModulesRendered(BrowserProxy.getInstance().now());
    }
  }

  /**
   * Set the #backgroundImage |path| only when different and non-empty. Reset
   * the customize dialog background selection if the dialog is closed.
   *
   * The ntp-untrusted-iframe |path| is set directly. When using a data binding
   * instead, the quick updates to the |path| result in iframe loading an error
   * page.
   * @private
   */
  updateBackgroundImagePath_() {
    // The |backgroundSelection_| is retained after the dialog commits the
    // change to the theme. Since |backgroundSelection_| has precedence over
    // the theme background, the |backgroundSelection_| needs to be reset when
    // the theme is updated. This is only necessary when the dialog is closed.
    // If the dialog is open, it will either commit the |backgroundSelection_|
    // or reset |backgroundSelection_| on cancel.
    //
    // Update after background image path is updated so the image is not shown
    // before the path is updated.
    if (!this.showCustomizeDialog_ &&
        this.backgroundSelection_.type !==
            BackgroundSelectionType.NO_SELECTION) {
      // Wait when local image is selected, then no background is previewed
      // followed by selecting a new local image. This avoids a flicker. The
      // iframe with the old image is shown briefly before it navigates to a new
      // iframe location, then fetches and renders the new local image.
      if (this.backgroundSelection_.type ===
          BackgroundSelectionType.NO_BACKGROUND) {
        setTimeout(() => {
          this.backgroundSelection_ = {
            type: BackgroundSelectionType.NO_SELECTION
          };
        }, 100);
      } else {
        this.backgroundSelection_ = {
          type: BackgroundSelectionType.NO_SELECTION
        };
      }
    }
    /** @type {newTabPage.mojom.BackgroundImage|undefined} */
    let backgroundImage;
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        backgroundImage = this.theme_ && this.theme_.backgroundImage;
        break;
      case BackgroundSelectionType.IMAGE:
        backgroundImage = {
          url: {url: this.backgroundSelection_.image.imageUrl.url}
        };
        break;
    }
    if (backgroundImage) {
      this.backgroundManager_.setBackgroundImage(backgroundImage);
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeDoodleAllowed_() {
    return loadTimeData.getBoolean('themeModeDoodlesEnabled') ||
        !this.showBackgroundImage_ && this.theme_ && this.theme_.isDefault &&
        !this.theme_.isDark;
  }

  /**
   * @return {skia.mojom.SkColor}
   * @private
   */
  computeBackgroundColor_() {
    if (this.showBackgroundImage_) {
      return null;
    }
    return this.theme_ && this.theme_.backgroundColor;
  }

  /**
   * @return {skia.mojom.SkColor}
   * @private
   */
  computeLogoColor_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return hexColorToSkColor('#ffffff');
      case BackgroundSelectionType.NO_SELECTION:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return this.theme_ &&
            (this.theme_.logoColor ||
             (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null));
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSingleColoredLogo_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
    }
  }

  /**
   * Sends the command received from the given source and origin to the browser.
   * Relays the browser response to whether or not a promo containing the given
   * command can be shown back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the response back to the source promo
   * frame and should not be used for anything else.
   * @param {Object} messageData Data received from the source promo frame.
   * @param {Window} commandSource Source promo frame.
   * @param {string} commandOrigin Origin of the source promo frame.
   * @private
   */
  canShowPromoWithBrowserCommand_(messageData, commandSource, commandOrigin) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!promoBrowserCommand.mojom.Command} */
    const commandId = Object.values(promoBrowserCommand.mojom.Command)
                          .includes(messageData.commandId) ?
        messageData.commandId :
        promoBrowserCommand.mojom.Command.kUnknownCommand;

    PromoBrowserCommandProxy.getInstance()
        .handler.canShowPromoWithCommand(commandId)
        .then(({canShow}) => {
          const response = {messageType: messageData.messageType};
          response[messageData.commandId] = canShow;
          commandSource.postMessage(response, commandOrigin);
        });
  }

  /**
   * Sends the command and the accompanying mouse click info received from the
   * promo of the given source and origin to the browser. Relays the execution
   * status response back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the execution status response back to
   * the source promo frame and should not be used for anything else.
   * @param {!CommandData} commandData Command and mouse click info.
   * @param {Window} commandSource Source promo frame.
   * @param {string} commandOrigin Origin of the source promo frame.
   * @private
   */
  executePromoBrowserCommand_(commandData, commandSource, commandOrigin) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!promoBrowserCommand.mojom.Command} */
    const commandId = Object.values(promoBrowserCommand.mojom.Command)
                          .includes(commandData.commandId) ?
        commandData.commandId :
        promoBrowserCommand.mojom.Command.kUnknownCommand;

    PromoBrowserCommandProxy.getInstance()
        .handler.executeCommand(commandId, commandData.clickInfo)
        .then(({commandExecuted}) => {
          commandSource.postMessage(commandExecuted, commandOrigin);
        });
  }

  /**
   * Handles messages from the OneGoogleBar iframe. The messages that are
   * handled include show bar on load and overlay updates.
   *
   * 'overlaysUpdated' message includes the updated array of overlay rects that
   * are shown.
   *
   * When modal overlays are enabled, activate/deactivate controls if the
   * OneGoogleBar is layered on top of #content with a backdrop. This would
   * happen when OneGoogleBar has an overlay open.
   * @param {!MessageEvent} event
   * @private
   */
  handleOneGoogleBarMessage_(event) {
    /** @type {!Object} */
    const data = event.data;
    if (data.messageType === 'loaded') {
      if (!this.oneGoogleBarModalOverlaysEnabled_) {
        const oneGoogleBar = $$(this, '#oneGoogleBar');
        oneGoogleBar.style.clipPath = 'url(#oneGoogleBarClipPath)';
        oneGoogleBar.style.zIndex = '1000';
      }
      this.oneGoogleBarLoaded_ = true;
      this.pageHandler_.onOneGoogleBarRendered(
          BrowserProxy.getInstance().now());
    } else if (data.messageType === 'overlaysUpdated') {
      this.$.oneGoogleBarClipPath.querySelectorAll('rect').forEach(el => {
        el.remove();
      });
      const overlayRects = /** @type {!Array<!DOMRect>} */ (data.data);
      overlayRects.forEach(({x, y, width, height}) => {
        const rectElement =
            document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        // Add 8px around every rect to ensure shadows are not cutoff.
        rectElement.setAttribute('x', x - 8);
        rectElement.setAttribute('y', y - 8);
        rectElement.setAttribute('width', width + 16);
        rectElement.setAttribute('height', height + 16);
        this.$.oneGoogleBarClipPath.appendChild(rectElement);
      });
    } else if (data.messageType === 'activate') {
      this.$.oneGoogleBarOverlayBackdrop.toggleAttribute('show', true);
      $$(this, '#oneGoogleBar').style.zIndex = '1000';
    } else if (data.messageType === 'deactivate') {
      this.$.oneGoogleBarOverlayBackdrop.toggleAttribute('show', false);
      $$(this, '#oneGoogleBar').style.zIndex = '0';
    } else if (data.messageType === 'can-show-promo-with-browser-command') {
      this.canShowPromoWithBrowserCommand_(data, event.source, event.origin);
    } else if (data.messageType === 'execute-browser-command') {
      this.executePromoBrowserCommand_(
          /** @type {!CommandData} */ (data.data), event.source, event.origin);
    }
  }

  /** @private */
  oneGoogleBarLoadedChange_() {
    if (this.oneGoogleBarLoaded_ && this.iframeOneGoogleBarEnabled_ &&
        this.oneGoogleBarModalOverlaysEnabled_) {
      this.setupShortcutDragDropOneGoogleBarWorkaround_();
    }
  }

  /** @private */
  onMiddleSlotPromoLoaded_() {
    this.middleSlotPromoLoaded_ = true;
    // The promo is always shown when modules are enabled since it will not
    // overlap with other elements.
    if (this.modulesEnabled_) {
      return;
    }
    const onResize = () => {
      const promoElement = $$(this, 'ntp-middle-slot-promo');
      promoElement.hidden =
          this.$.mostVisited.getBoundingClientRect().bottom >=
          promoElement.offsetTop;
    };
    this.eventTracker_.add(window, 'resize', onResize);
    onResize();
  }

  /** @private */
  onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  /**
   * @param {!CustomEvent<string>} e Event notifying a module was dismissed.
   *     Contains the message to show in the toast.
   * @private
   */
  onDismissModule_(e) {
    this.dismissedModuleWrapper_ = /** @type {!Element} */ (e.target);

    // Notify the user.
    this.dismissModuleToastMessage_ = e.detail;
    $$(this, '#dismissModuleToast').show();
    // Notify the backend.
    this.pageHandler_.onDismissModule(
        this.dismissedModuleWrapper_.descriptor.id);
  }

  /**
   * @private
   */
  onUndoDismissModuleButtonClick_() {
    // Restore the module.
    this.dismissedModuleWrapper_.restore();
    // Notify the user.
    $$(this, '#dismissModuleToast').hide();
    // Notify the backend.
    this.pageHandler_.onRestoreModule(
        this.dismissedModuleWrapper_.descriptor.id);

    this.dismissedModuleWrapper_ = null;
  }

  /**
   * During a shortcut drag, an iframe behind ntp-most-visited will prevent
   * 'dragover' events from firing. To workaround this, 'pointer-events: none'
   * can be set on the iframe. When doing this after the 'dragstart' event is
   * fired is too late. We can instead set 'pointer-events: none' when the
   * pointer enters ntp-most-visited.
   *
   * 'pointerenter' and pointerleave' events fire during drag. The iframe
   * 'pointer-events' needs to be reset to the original value when 'dragend'
   * fires if the pointer has left ntp-most-visited.
   * @private
   */
  setupShortcutDragDropOneGoogleBarWorkaround_() {
    const iframe = $$(this, '#oneGoogleBar');
    let resetAtDragEnd = false;
    let dragging = false;
    let originalPointerEvents;
    this.eventTracker_.add(this.$.mostVisited, 'pointerenter', () => {
      if (dragging) {
        resetAtDragEnd = false;
        return;
      }
      originalPointerEvents = getComputedStyle(iframe).pointerEvents;
      iframe.style.pointerEvents = 'none';
    });
    this.eventTracker_.add(this.$.mostVisited, 'pointerleave', () => {
      if (dragging) {
        resetAtDragEnd = true;
        return;
      }
      iframe.style.pointerEvents = originalPointerEvents;
    });
    this.eventTracker_.add(this.$.mostVisited, 'dragstart', () => {
      dragging = true;
    });
    this.eventTracker_.add(this.$.mostVisited, 'dragend', () => {
      dragging = false;
      if (resetAtDragEnd) {
        resetAtDragEnd = false;
        iframe.style.pointerEvents = originalPointerEvents;
      }
    });
  }

  /** @private */
  printPerformanceDatum_(name, time, auxTime = 0) {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    if (!auxTime) {
      console.log(`${name}: ${time}`);
    } else {
      console.log(`${name}: ${time} (${auxTime})`);
    }
  }

  /**
   * Prints performance measurements to the console. Also, installs  performance
   * observer to continuously print performance measurements after.
   * @private
   */
  printPerformance_() {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    const entryTypes = ['paint', 'measure'];
    const log = (entry) => {
      this.printPerformanceDatum_(
          entry.name, entry.duration ? entry.duration : entry.startTime,
          entry.duration && entry.startTime ? entry.startTime : 0);
    };
    const observer = new PerformanceObserver(list => {
      list.getEntries().forEach((entry) => {
        log(entry);
      });
    });
    observer.observe({entryTypes: entryTypes});
    performance.getEntries().forEach((entry) => {
      if (!entryTypes.includes(entry.entryType)) {
        return;
      }
      log(entry);
    });
  }
}

customElements.define(AppElement.is, AppElement);
