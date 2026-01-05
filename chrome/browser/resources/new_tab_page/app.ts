// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './action_chips/action_chips.js';
import './iframe.js';
import './logo.js';
import '/strings.m.js';
import 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
import 'chrome://resources/cr_components/searchbox/searchbox.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import {GlifAnimationState} from '//resources/cr_components/composebox/context_menu_entrypoint.js';
import type {CustomizeButtonsElement} from 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ContextualUpload} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {VoiceSearchAction as ComposeVoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox.js';
import {ComposeboxMode} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {SearchboxElement} from 'chrome://resources/cr_components/searchbox/searchbox.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {ClickInfo} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {Command} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ActionChipsRetrievalState} from './action_chips/action_chips.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BackgroundManager} from './background_manager.js';
import type {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote} from './customize_buttons.mojom-webui.js';
import {SidePanelOpenTrigger} from './customize_buttons.mojom-webui.js';
import {CustomizeButtonsProxy} from './customize_buttons_proxy.js';
import {CustomizeChromeSection} from './customize_chrome.mojom-webui.js';
import {CustomizeDialogPage} from './customize_dialog_types.js';
import type {IframeElement} from './iframe.js';
import type {LogoElement} from './logo.js';
import {recordBoolean, recordDuration, recordEnumeration, recordLinearValue, recordLoadDuration, recordSparseValueWithPersistentHash} from './metrics_utils.js';
import {ParentTrustedDocumentProxy} from './modules/microsoft_auth_frame_connector.js';
import type {PageCallbackRouter, PageHandlerRemote, Theme} from './new_tab_page.mojom-webui.js';
import {NtpBackgroundImageSource} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import type {MicrosoftAuthUntrustedDocumentRemote} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {ShowNtpPromosResult} from './ntp_promo.mojom-webui.js';
import {$$} from './utils.js';
import {Action as VoiceAction, recordVoiceAction} from './voice_search_overlay.js';
import {WindowProxy} from './window_proxy.js';

enum ModuleLoadStatus {
  MODULE_LOAD_IN_PROGRESS = 0,
  MODULE_LOAD_NOT_ATTEMPTED = 1,
  MODULE_LOAD_COMPLETE = 2,
}

interface ExecutePromoBrowserCommandData {
  commandId: Command;
  clickInfo: ClickInfo;
}

interface CanShowPromoWithBrowserCommandData {
  frameType: string;
  messageType: string;
  commandId: Command;
}

/**
 * Elements on the NTP. This enum must match the numbering for NTPElement in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 */
export enum NtpElement {
  OTHER = 0,
  BACKGROUND = 1,
  ONE_GOOGLE_BAR = 2,
  LOGO = 3,
  REALBOX = 4,
  MOST_VISITED = 5,
  MIDDLE_SLOT_PROMO = 6,
  MODULE = 7,
  CUSTOMIZE = 8,  // Obsolete
  CUSTOMIZE_BUTTON = 9,
  CUSTOMIZE_DIALOG = 10,  // Obsolete
  WALLPAPER_SEARCH_BUTTON = 11,
  ACTION_CHIPS = 12,
  MAX_VALUE = ACTION_CHIPS,
}

/**
 * Customize Chrome entry points. This enum must match the numbering for
 * NtpCustomizeChromeEntryPoint in enums.xml. These values are persisted to
 * logs. Entries should not be renumbered, removed or reused.
 */
export enum NtpCustomizeChromeEntryPoint {
  CUSTOMIZE_BUTTON = 0,
  MODULE = 1,
  URL = 2,
  WALLPAPER_SEARCH_BUTTON = 3,
  MAX_VALUE = WALLPAPER_SEARCH_BUTTON,
}

/**
 * Defines the conditions that hide the wallpaper search button on the New Tab
 * Page.
 */
enum NtpWallpaperSearchButtonHideCondition {
  NONE = 0,
  BACKGROUND_IMAGE_SET = 1,
  THEME_SET = 2,
  MAX_VALUE = THEME_SET,
}

const CUSTOMIZE_URL_PARAM: string = 'customize';
const OGB_IFRAME_ORIGIN = 'chrome-untrusted://new-tab-page';
const MSAL_IFRAME_ORIGIN = 'chrome-untrusted://ntp-microsoft-auth';

export const CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID =
    'CustomizeButtonsHandler::kCustomizeChromeButtonElementId';

// 900px ~= 561px (max value for --ntp-search-box-width) * 1.5 + some margin.
const realboxCanShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 900px)');

function recordClick(element: NtpElement) {
  recordEnumeration('NewTabPage.Click', element, NtpElement.MAX_VALUE + 1);
}

function recordCustomizeChromeOpen(element: NtpCustomizeChromeEntryPoint) {
  recordEnumeration(
      'NewTabPage.CustomizeChromeOpened', element,
      NtpCustomizeChromeEntryPoint.MAX_VALUE + 1);
}

// Adds a <script> tag that holds the lazy loaded code.
function ensureLazyLoaded() {
  const script = document.createElement('script');
  script.type = 'module';
  script.src = getTrustedScriptURL`./lazy_load.js`;
  document.body.appendChild(script);
}

function recordShowBrowserPromosResult(result: ShowNtpPromosResult) {
  recordEnumeration(
      'UserEducation.NtpPromos.ShowResult', result,
      ShowNtpPromosResult.MAX_VALUE + 1);
}

const AppElementBase = HelpBubbleMixinLit(CrLitElement);

export interface AppElement {
  $: {
    customizeButtons: CustomizeButtonsElement,
    oneGoogleBarClipPath: HTMLElement,
    logo: LogoElement,
    searchbox: SearchboxElement,
    composebox: ComposeboxElement,
    undoToast: CrToastElement,
    undoToastMessage: HTMLElement,
  };
}

export class AppElement extends AppElementBase {
  static get is() {
    return 'ntp-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      oneGoogleBarIframeOrigin_: {type: String},
      oneGoogleBarIframePath_: {type: String},
      oneGoogleBarLoaded_: {type: Boolean},
      theme_: {type: Object},
      showCustomize_: {type: Boolean},
      showCustomizeChromeText_: {type: Boolean},

      showWallpaperSearch_: {type: Boolean},

      isActionChipsVisible_: {type: Boolean},

      isFooterVisible_: {type: Boolean},

      selectedCustomizeDialogPage_: {type: String},
      showVoiceSearchOverlay_: {type: Boolean},

      showBackgroundImage_: {
        reflect: true,
        type: Boolean,
      },

      backgroundImageAttribution1_: {type: String},
      backgroundImageAttribution2_: {type: String},
      backgroundImageAttributionUrl_: {type: String},

      // Used in cr-searchbox component via host-context.
      colorSourceIsBaseline: {type: Boolean},
      logoColor_: {type: String},
      singleColoredLogo_: {type: Boolean},

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown for the ntp searchbox.
       */
      realboxCanShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the searchbox secondary side was at any point available to
       * be shown.
       */
      realboxHadSecondarySide: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      composeboxCloseByClickOutside_: {type: Boolean},
      composeboxEnabled: {type: Boolean},
      composeButtonEnabled: {type: Boolean},

      browserPromoType_: {type: String},
      browserPromoLimit_: {type: Number},
      browserPromoCompletedLimit_: {type: Number},
      showBrowserPromo_: {type: Boolean},

      realboxShown_: {type: Boolean},
      logoEnabled_: {type: Boolean},
      oneGoogleBarEnabled_: {type: Boolean},
      shortcutsEnabled_: {type: Boolean},
      middleSlotPromoEnabled_: {type: Boolean},
      modulesEnabled_: {type: Boolean},
      middleSlotPromoLoaded_: {type: Boolean},
      modulesLoadedStatus_: {
        type: Number,
        reflect: true,
      },

      modulesShownToUser: {
        type: Boolean,
        reflect: true,
      },

      microsoftModuleEnabled_: {type: Boolean},
      microsoftAuthIframePath_: {type: String},

      multiLineEnabled_: {type: Boolean},

      ntpRealboxNextEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * In order to avoid flicker, the promo and modules are hidden until both
       * are loaded. If modules are disabled, the promo is shown as soon as it
       * is loaded.
       */
      promoAndModulesLoaded_: {type: Boolean},

      realboxLayoutMode_: {
        type: String,
        reflect: true,
      },

      searchboxCyclingPlaceholders_: {
        type: Boolean,
      },

      showComposebox_: {
        type: Boolean,
        reflect: true,
      },

      showLensUploadDialog_: {type: Boolean},

      /**
       * If true, renders additional elements that were not deemed crucial to
       * to show up immediately on load.
       */
      lazyRender_: {type: Boolean},

      scrolledToTop_: {type: Boolean},

      wallpaperSearchButtonAnimationEnabled_: {type: Boolean},

      wallpaperSearchButtonEnabled_: {type: Boolean},

      showWallpaperSearchButton_: {type: Boolean},

      /**
       * Whether the composebox has been opened at least once.
       */
      wasComposeboxOpened_: {type: Boolean},

      ntpNextFeaturesEnabled_: {type: Boolean},
      maxTilesBeforeShowMore_: {type: Number},

      searchboxInputFocused_: {type: Boolean},
      composeboxInputFocused_: {type: Boolean},
      /**
       * Whether the scrim is shown in Realbox Next.
       */
      showScrim_: {type: Boolean, reflect: true},

      contextMenuGlifAnimationState_: {type: String},
      undoAutoRemovalCallback_: {type: Object},
      undoAutoRemovalMessage_: {type: Object},
    };
  }

  protected accessor oneGoogleBarIframeOrigin_: string = OGB_IFRAME_ORIGIN;
  protected accessor oneGoogleBarIframePath_: string|undefined;
  protected accessor oneGoogleBarLoaded_: boolean = false;
  protected accessor theme_: Theme|null = null;
  protected accessor showCustomize_: boolean = false;
  protected accessor showCustomizeChromeText_: boolean = false;
  protected accessor showWallpaperSearch_: boolean = false;
  private accessor selectedCustomizeDialogPage_: string|null = null;
  protected accessor showVoiceSearchOverlay_: boolean = false;
  protected accessor showBackgroundImage_: boolean = false;
  protected accessor backgroundImageAttribution1_: string = '';
  protected accessor backgroundImageAttribution2_: string = '';
  protected accessor backgroundImageAttributionUrl_: string = '';
  protected accessor colorSourceIsBaseline: boolean = false;
  protected accessor logoColor_: SkColor|null = null;
  protected accessor singleColoredLogo_: boolean = false;
  accessor realboxCanShowSecondarySide: boolean = false;
  accessor realboxHadSecondarySide: boolean = false;
  protected accessor realboxShown_: boolean = false;
  protected accessor wasComposeboxOpened_: boolean = false;
  protected accessor showLensUploadDialog_: boolean = false;
  protected accessor showComposebox_: boolean = false;
  protected accessor logoEnabled_: boolean =
      loadTimeData.getBoolean('logoEnabled');
  protected accessor oneGoogleBarEnabled_: boolean =
      loadTimeData.getBoolean('oneGoogleBarEnabled');
  protected accessor shortcutsEnabled_: boolean =
      loadTimeData.getBoolean('shortcutsEnabled');
  protected accessor middleSlotPromoEnabled_: boolean =
      loadTimeData.getBoolean('middleSlotPromoEnabled');
  protected accessor modulesEnabled_: boolean =
      loadTimeData.getBoolean('modulesEnabled');
  protected accessor browserPromoType_: string =
      loadTimeData.getString('browserPromoType');
  protected accessor browserPromoLimit_: number =
      loadTimeData.getInteger('browserPromoLimit');
  protected accessor browserPromoCompletedLimit_: number =
      loadTimeData.getInteger('browserPromoCompletedLimit');
  protected accessor showBrowserPromo_: boolean = false;
  private accessor middleSlotPromoLoaded_: boolean = false;
  private accessor modulesLoadedStatus_: ModuleLoadStatus =
      ModuleLoadStatus.MODULE_LOAD_IN_PROGRESS;
  protected accessor modulesShownToUser: boolean = false;
  protected accessor microsoftModuleEnabled_: boolean =
      loadTimeData.getBoolean('microsoftModuleEnabled');
  protected accessor microsoftAuthIframePath_: string = MSAL_IFRAME_ORIGIN;
  protected accessor multiLineEnabled_: boolean =
      loadTimeData.getBoolean('multiLineEnabled');
  protected accessor promoAndModulesLoaded_: boolean = false;
  protected accessor lazyRender_: boolean = false;
  protected accessor scrolledToTop_: boolean =
      document.documentElement.scrollTop <= 0;
  protected accessor wallpaperSearchButtonAnimationEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchButtonAnimationEnabled');
  protected accessor wallpaperSearchButtonEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchButtonEnabled');
  protected accessor showWallpaperSearchButton_: boolean = false;
  accessor composeButtonEnabled: boolean =
      loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
  protected accessor composeboxCloseByClickOutside_: boolean =
      loadTimeData.getBoolean('composeboxCloseByClickOutside');
  accessor composeboxEnabled: boolean =
      loadTimeData.getBoolean('searchboxShowComposebox');
  protected accessor isActionChipsVisible_: boolean =
      loadTimeData.getBoolean('actionChipsEnabled');
  protected accessor isFooterVisible_: boolean = false;
  protected accessor ntpRealboxNextEnabled_: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');
  protected accessor realboxLayoutMode_: string =
      loadTimeData.getString('realboxLayoutMode');
  protected accessor searchboxCyclingPlaceholders_: boolean =
      loadTimeData.getBoolean('searchboxCyclingPlaceholders');
  protected accessor ntpNextFeaturesEnabled_: boolean =
      loadTimeData.getBoolean('ntpNextFeaturesEnabled');
  protected accessor maxTilesBeforeShowMore_: number =
      loadTimeData.getInteger('maxTilesBeforeShowMore');
  protected accessor searchboxInputFocused_: boolean = false;
  protected accessor composeboxInputFocused_: boolean = false;
  protected accessor showScrim_: boolean = false;
  protected accessor contextMenuGlifAnimationState_: GlifAnimationState =
      this.ntpNextFeaturesEnabled_ && this.isActionChipsVisible_ ?
      GlifAnimationState.SPINNER_ONLY :
      GlifAnimationState.INELIGIBLE;
  protected accessor undoAutoRemovalCallback_: (() => void)|null = null;
  protected accessor undoAutoRemovalMessage_: string|null = null;
  protected enableModalComposebox_: boolean =
      loadTimeData.getBoolean('enableModalComposebox');
  protected ephemeralContextMenuDescriptionEnabled_: boolean =
      loadTimeData.getBoolean('enableEphemeralContextMenuDescription') ?? false;
  protected showContextMenuDescription_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');

  private callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private customizeButtonsCallbackRouter_:
      CustomizeButtonsDocumentCallbackRouter;
  private customizeButtonsHandler_: CustomizeButtonsHandlerRemote;
  private backgroundManager_: BackgroundManager;
  private connectMicrosoftAuthToParentDocumentListenerId_: number|null = null;
  private setThemeListenerId_: number|null = null;
  private setCustomizeChromeSidePanelVisibilityListener_: number|null = null;
  private setWallpaperSearchButtonVisibilityListener_: number|null = null;
  private setActionChipsVisibilityListenerId_: number|null = null;
  private footerVisibilityUpdatedListener_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private shouldPrintPerformance_: boolean = false;
  private backgroundImageLoadStartEpoch_: number = 0;
  private backgroundImageLoadStart_: number = 0;
  private showWebstoreToastListenerId_: number|null = null;
  private pendingComposeboxContextFiles_: ContextualUpload[] = [];
  private pendingComposeboxText_: string = '';
  private pendingComposeboxMode_: ComposeboxMode = ComposeboxMode.DEFAULT;
  private pendingAutoRemovalToasts_:
      Array<{message: string, undo: () => void}> = [];

  constructor() {
    performance.mark('app-creation-start');
    super();
    this.callbackRouter_ = NewTabPageProxy.getInstance().callbackRouter;
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.customizeButtonsCallbackRouter_ =
        CustomizeButtonsProxy.getInstance().callbackRouter;
    this.customizeButtonsHandler_ = CustomizeButtonsProxy.getInstance().handler;
    this.backgroundManager_ = BackgroundManager.getInstance();
    this.shouldPrintPerformance_ =
        new URLSearchParams(location.search).has('print_perf');

    this.oneGoogleBarIframePath_ = (() => {
      const params = new URLSearchParams();
      params.set(
          'paramsencoded', btoa(window.location.search.replace(/^[?]/, '&')));
      return `${OGB_IFRAME_ORIGIN}/one-google-bar?${params}`;
    })();

    this.showCustomize_ =
        WindowProxy.getInstance().url.searchParams.has(CUSTOMIZE_URL_PARAM);

    this.selectedCustomizeDialogPage_ =
        WindowProxy.getInstance().url.searchParams.get(CUSTOMIZE_URL_PARAM);

    this.realboxCanShowSecondarySide =
        realboxCanShowSecondarySideMediaQueryList.matches;

    /**
     * Initialized with the start of the performance timeline in case the
     * background image load is not triggered by JS.
     */
    this.backgroundImageLoadStartEpoch_ = performance.timeOrigin;

    recordLinearValue(
        'NewTabPage.Height',
        /*min=*/ 1,
        /*max=*/ 1000,
        /*buckets=*/ 200,
        /*value=*/ Math.floor(window.innerHeight));
    recordLinearValue(
        'NewTabPage.Width',
        /*min=*/ 1,
        /*max=*/ 1920,
        /*buckets=*/ 384,
        /*value=*/ Math.floor(window.innerWidth));

    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    realboxCanShowSecondarySideMediaQueryList.addEventListener(
        'change', this.onRealboxCanShowSecondarySideChanged_.bind(this));

    // Listen for chrome-untrusted://ntp-microsoft-auth iframe trying to
    // connect to the NTP.
    this.connectMicrosoftAuthToParentDocumentListenerId_ =
        this.callbackRouter_.connectToParentDocument.addListener(
            (childDocumentRemote: MicrosoftAuthUntrustedDocumentRemote) => {
              ParentTrustedDocumentProxy.setInstance(childDocumentRemote);
            });

    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          if (!this.theme_) {
            this.onThemeLoaded_(theme);
          }
          performance.measure('theme-set');
          this.theme_ = theme;
        });
    this.setCustomizeChromeSidePanelVisibilityListener_ =
        this.customizeButtonsCallbackRouter_
            .setCustomizeChromeSidePanelVisibility.addListener(
                (visible: boolean) => {
                  this.showCustomize_ = visible;
                  if (!visible) {
                    this.showWallpaperSearch_ = false;
                  }
                });
    this.showWebstoreToastListenerId_ =
        this.callbackRouter_.showWebstoreToast.addListener(() => {
          if (this.showCustomize_) {
            const toast = $$<CrToastElement>(this, '#webstoreToast');
            if (toast) {
              toast.hidden = false;
              toast.show();
            }
          }
        });
    this.setWallpaperSearchButtonVisibilityListener_ =
        this.callbackRouter_.setWallpaperSearchButtonVisibility.addListener(
            (visible: boolean) => {
              // Hides the wallpaper search button if the browser indicates that
              // it should be hidden.
              // Note: We don't resurface the button later even if the browser
              // says we should, to avoid issues if Customize Chrome doesn't
              // have the wallpaper search element yet.
              if (!visible) {
                this.wallpaperSearchButtonEnabled_ = visible;
              }
            });

    this.setActionChipsVisibilityListenerId_ =
        this.callbackRouter_.setActionChipsVisibility.addListener(
            (isVisible: boolean) => this.isActionChipsVisible_ = isVisible);

    this.footerVisibilityUpdatedListener_ =
        this.callbackRouter_.footerVisibilityUpdated.addListener(
            (visible: boolean) => {
              this.isFooterVisible_ = visible;
            });
    this.pageHandler_.updateFooterVisibility();

    // Open Customize Chrome if there are Customize Chrome URL params.
    if (this.showCustomize_) {
      this.setCustomizeChromeSidePanelVisible_(this.showCustomize_);
      recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.URL);
    }
    this.eventTracker_.add(window, 'message', (event: MessageEvent) => {
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
    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));
    this.eventTracker_.add(
        window, 'click', this.onWindowClick_.bind(this), /*capture=*/ true);
    this.eventTracker_.add(document, 'scroll', () => {
      this.scrolledToTop_ = document.documentElement.scrollTop <= 0;
    });
    if (loadTimeData.getString('backgroundImageUrl')) {
      this.backgroundManager_.getBackgroundImageLoadTime().then(
          time => {
            const duration = time - this.backgroundImageLoadStartEpoch_;
            recordDuration(
                'NewTabPage.Images.ShownTime.BackgroundImage', duration);
            if (this.shouldPrintPerformance_) {
              this.printPerformanceDatum_(
                  'background-image-load', this.backgroundImageLoadStart_,
                  duration);
              this.printPerformanceDatum_(
                  'background-image-loaded',
                  this.backgroundImageLoadStart_ + duration);
            }
          },
          () => {
              // Ignore. Failed to capture background image load time.
          });
    }
    FocusOutlineManager.forDocument(document);
    if (this.composeButtonEnabled) {
      recordBoolean('NewTabPage.ComposeEntrypoint.Shown', true);
      this.pageHandler_.incrementComposeButtonShownCount();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    realboxCanShowSecondarySideMediaQueryList.removeEventListener(
        'change', this.onRealboxCanShowSecondarySideChanged_.bind(this));
    this.callbackRouter_.removeListener(
        this.connectMicrosoftAuthToParentDocumentListenerId_!);
    this.callbackRouter_.removeListener(this.setThemeListenerId_!);
    this.callbackRouter_.removeListener(this.showWebstoreToastListenerId_!);
    this.callbackRouter_.removeListener(
        this.setWallpaperSearchButtonVisibilityListener_!);
    this.customizeButtonsCallbackRouter_.removeListener(
        this.setCustomizeChromeSidePanelVisibilityListener_!);
    this.callbackRouter_.removeListener(
        this.setActionChipsVisibilityListenerId_!);
    this.callbackRouter_.removeListener(this.footerVisibilityUpdatedListener_!);
    this.eventTracker_.removeAll();
  }

  override firstUpdated() {
    this.pageHandler_.onAppRendered(WindowProxy.getInstance().now());
    // Let the browser breathe and then render remaining elements.
    WindowProxy.getInstance().waitForLazyRender().then(() => {
      ensureLazyLoaded();
      this.lazyRender_ = true;
    });
    this.printPerformance_();
    performance.measure('app-creation', 'app-creation-start');

    if (!this.modulesEnabled_) {
      this.recordBrowserPromoMetrics_();
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('theme_')) {
      this.showBackgroundImage_ = this.computeShowBackgroundImage_();
      this.backgroundImageAttribution1_ =
          this.computeBackgroundImageAttribution1_();
      this.backgroundImageAttribution2_ =
          this.computeBackgroundImageAttribution2_();
      this.backgroundImageAttributionUrl_ =
          this.computeBackgroundImageAttributionUrl_();
      this.colorSourceIsBaseline = this.computeColorSourceIsBaseline();
      this.logoColor_ = this.computeLogoColor_();
      this.singleColoredLogo_ = this.computeSingleColoredLogo_();
    }

    // theme_, showLensUploadDialog_
    this.realboxShown_ = this.computeRealboxShown_();

    // middleSlotPromoLoaded_, modulesLoadedStatus_
    this.promoAndModulesLoaded_ = this.computePromoAndModulesLoaded_();

    // wallpaperSearchButtonEnabled_, showBackgroundImage_, backgroundColor_
    this.showWallpaperSearchButton_ = this.computeShowWallpaperSearchButton_();

    // showWallpaperSearchButton_, showBackgroundImage_
    this.showCustomizeChromeText_ = this.computeShowCustomizeChromeText_();

    // modulesEnabled_, modulesShownToUser, modulesLoadedStatus_
    this.showBrowserPromo_ = this.computeShowBrowserPromo_();

    if ((changedPrivateProperties.has('modulesLoadedStatus_') &&
         this.modulesLoadedStatus_ !==
             ModuleLoadStatus.MODULE_LOAD_IN_PROGRESS)) {
      this.recordBrowserPromoMetrics_();
    }

    if (this.ntpRealboxNextEnabled_ && [
          'showComposebox_',
          'searchboxInputFocused_',
          'composeboxInputFocused_',
        ].some((prop) => changedPrivateProperties.has(prop))) {
      /**
       * The current requirement is that the scrim should be shown when the
       * focus is placed on one of the input boxes and should be removed when
       * the focus moves outside.
       *
       * The additional OR operation with showComposebox_ is because the logic
       * does not close Composebox when a click outside is made while Composebox
       * is opened. What seems to be happening when showComposebox_ is used/not
       * used are as follows:
       * - Without it:
       *   1. A click outside is made.
       *   2. The focusout event first occurs.
       *   3. composeboxInputFocused_ is set to false.
       *   4. The scrim is removed.
       *   5. The click event fires.
       *   6. Since there is no scrim, the onclick handle of the scrim is not
       *      called.
       * - With it:
       *   1-3. same as above
       *   4. The scrim is kept since showComposebox_ is still true.
       *   5. The onclick handler of the scrim runs and sets showComposebox_ to
       *      false, and everything works as desired.
       */
      this.showScrim_ = this.showComposebox_ || this.searchboxInputFocused_ ||
          this.composeboxInputFocused_;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('lazyRender_') && this.lazyRender_) {
      this.onLazyRendered_();
    }

    if (changedPrivateProperties.has('theme_')) {
      this.onThemeChange_();
    }

    if (changedPrivateProperties.has('isFooterVisible_') && this.lazyRender_) {
      this.maybeRegisterCustomizeButtonHelpBubble_();
    }

    if (changedPrivateProperties.has('logoColor_')) {
      this.style.setProperty(
          '--ntp-logo-color', this.rgbaOrInherit_(this.logoColor_));
    }

    if (changedPrivateProperties.has('showBackgroundImage_')) {
      this.onShowBackgroundImageChange_();
    }

    if (changedPrivateProperties.has('promoAndModulesLoaded_')) {
      this.onPromoAndModulesLoadedChange_();
    }

    if (changedPrivateProperties.has('showComposebox_') &&
        this.showComposebox_ && this.enableModalComposebox_) {
      const composeboxDialog =
          this.shadowRoot.querySelector<HTMLDialogElement>('#composeboxDialog');
      assert(composeboxDialog);
      composeboxDialog.showModal();
    }

    if (changedPrivateProperties.has('oneGoogleBarLoaded_') ||
        changedPrivateProperties.has('theme_') ||
        changedPrivateProperties.has('showComposebox_')) {
      this.updateOneGoogleBarAppearance_();
    }
  }

  // Called to update the OGB of relevant NTP state changes.
  private updateOneGoogleBarAppearance_() {
    if (this.oneGoogleBarLoaded_) {
      let isNtpDarkTheme;
      if (this.showComposebox_) {
        isNtpDarkTheme = this.theme_ && this.theme_.isDark;
      } else {
        isNtpDarkTheme = this.theme_ &&
            (!!this.theme_.backgroundImage || this.theme_.isDark);
      }
      $$<IframeElement>(this, '#oneGoogleBar')!.postMessage({
        type: 'updateAppearance',
        // We should be using a light OGB for dark themes and vice versa.
        applyLightTheme: isNtpDarkTheme,
      });
    }
  }

  private computeShowCustomizeChromeText_(): boolean {
    if (this.showWallpaperSearchButton_) {
      return false;
    }
    return !this.showBackgroundImage_;
  }

  private computeBackgroundImageAttribution1_(): string {
    return this.theme_ && this.theme_.backgroundImageAttribution1 || '';
  }

  private computeBackgroundImageAttribution2_(): string {
    return this.theme_ && this.theme_.backgroundImageAttribution2 || '';
  }

  private computeBackgroundImageAttributionUrl_(): string {
    return this.theme_ && this.theme_.backgroundImageAttributionUrl ?
        this.theme_.backgroundImageAttributionUrl.url :
        '';
  }

  private computeRealboxShown_(): boolean {
    // Do not show the realbox if the upload dialog is showing.
    return !!this.theme_ && !this.showLensUploadDialog_ &&
        !this.showComposebox_;
  }

  private computePromoAndModulesLoaded_(): boolean {
    return (!loadTimeData.getBoolean('middleSlotPromoEnabled') ||
            this.middleSlotPromoLoaded_) &&
        (!loadTimeData.getBoolean('modulesEnabled') ||
         this.modulesLoadedStatus_ === ModuleLoadStatus.MODULE_LOAD_COMPLETE);
  }

  private onRealboxCanShowSecondarySideChanged_(e: MediaQueryListEvent) {
    this.realboxCanShowSecondarySide = e.matches;
  }

  private onLazyRendered_() {
    // Integration tests use this attribute to determine when lazy load has
    // completed.
    document.documentElement.setAttribute('lazy-loaded', String(true));
    if (this.maybeRegisterCustomizeButtonHelpBubble_()) {
      this.pageHandler_.maybeTriggerAutomaticCustomizeChromePromo();
    }
    if (this.showWallpaperSearchButton_) {
      this.customizeButtonsHandler_.incrementWallpaperSearchButtonShownCount();
    }
  }

  private maybeRegisterCustomizeButtonHelpBubble_(): boolean {
    if (!this.isFooterVisible_) {
      this.registerHelpBubble(
          CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID,
          ['ntp-customize-buttons', '#customizeButton'], {fixed: true});
      return true;
    }
    return false;
  }

  protected onComposeboxInitialized_(e: CustomEvent<{
    initializeComposeboxState:
        (text: string, files: ContextualUpload[], mode: ComposeboxMode) => void,
  }>) {
    e.detail.initializeComposeboxState(
        this.pendingComposeboxText_, this.pendingComposeboxContextFiles_,
        this.pendingComposeboxMode_);
    this.pendingComposeboxContextFiles_ = [];
    this.pendingComposeboxText_ = '';
    this.pendingComposeboxMode_ = ComposeboxMode.DEFAULT;
  }

  protected openComposebox_(e: CustomEvent<{
    searchboxText: string,
    contextFiles: ContextualUpload[],
    mode: ComposeboxMode,
  }>) {
    if (e.detail.searchboxText) {
      this.pendingComposeboxText_ = e.detail.searchboxText;
    }
    if (e.detail.contextFiles && e.detail.contextFiles.length > 0) {
      this.pendingComposeboxContextFiles_ = e.detail.contextFiles;
    }
    this.pendingComposeboxMode_ = e.detail.mode;
    this.toggleComposebox_();
  }

  protected toggleComposebox_() {
    this.showComposebox_ = !this.showComposebox_;
    if (!this.wasComposeboxOpened_) {
      recordLoadDuration(
          'NewTabPage.Composebox.FromNTPLoadToSessionStart',
          WindowProxy.getInstance().now());
      this.wasComposeboxOpened_ = true;
    }
  }

  protected onComposeboxClickOutside_() {
    const composebox =
        this.shadowRoot.querySelector<ComposeboxElement>('#composebox');
    assert(composebox);
    const closeComposebox = new CustomEvent('closeComposebox', {
      detail: {composeboxText: composebox.getText()},
      bubbles: true,
      cancelable: true,
    });

    this.closeComposebox_(closeComposebox);
  }

  protected closeComposebox_(e: CustomEvent) {
    if (this.enableModalComposebox_) {
      const composeboxDialog =
          this.shadowRoot.querySelector<HTMLDialogElement>('#composeboxDialog');
      assert(composeboxDialog);
      composeboxDialog.close();
    }

    const composeboxText = e.detail.composeboxText;

    if (composeboxText && composeboxText.trim()) {
      this.$.searchbox.setInputText(composeboxText);
    }
    const composebox =
        this.shadowRoot.querySelector<ComposeboxElement>('#composebox');
    assert(composebox);
    composebox.setText('');
    composebox.resetModes();
    if (this.ntpRealboxNextEnabled_) {
      composebox.closeDropdown();
    }
    this.toggleComposebox_();
    this.logoColor_ = this.computeLogoColor_();
    this.singleColoredLogo_ = this.computeSingleColoredLogo_();
    this.updateOneGoogleBarAppearance_();
  }

  protected onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    recordVoiceAction(VoiceAction.ACTIVATE);
  }

  protected onComposeVoiceSearchAction_(
      e: CustomEvent<{value: ComposeVoiceSearchAction}>) {
    switch (e.detail.value) {
      case ComposeVoiceSearchAction.ACTIVATE:
        recordVoiceAction(VoiceAction.ACTIVATE);
        break;
      case ComposeVoiceSearchAction.QUERY_SUBMITTED:
        recordVoiceAction(VoiceAction.QUERY_SUBMITTED);
        break;
      default:
        assertNotReached();
    }
  }

  protected onOpenLensSearch_() {
    this.showLensUploadDialog_ = true;
  }

  protected onCloseLensSearch_() {
    this.showLensUploadDialog_ = false;
  }

  protected onContextMenuEntrypointClick_() {
    if (this.ephemeralContextMenuDescriptionEnabled_ &&
        this.showContextMenuDescription_) {
      this.pageHandler_.recordContextMenuClick();
    }
  }

  protected onCustomizeClick_() {
    // Let side panel decide what page or section to show.
    this.selectedCustomizeDialogPage_ = null;
    this.setCustomizeChromeSidePanelVisible_(!this.showCustomize_);
    if (!this.showCustomize_) {
      this.customizeButtonsHandler_.incrementCustomizeChromeButtonOpenCount();
      recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON);
    }
  }

  protected computeShowWallpaperSearchButton_() {
    if (!this.wallpaperSearchButtonEnabled_) {
      return false;
    }

    switch (loadTimeData.getInteger('wallpaperSearchButtonHideCondition')) {
      case NtpWallpaperSearchButtonHideCondition.NONE:
        return true;
      case NtpWallpaperSearchButtonHideCondition.BACKGROUND_IMAGE_SET:
        return !this.showBackgroundImage_;
      case NtpWallpaperSearchButtonHideCondition.THEME_SET:
        return this.colorSourceIsBaseline && !this.showBackgroundImage_;
    }
    return false;
  }

  protected onWallpaperSearchClick_() {
    // Close the side panel if Wallpaper Search is open.
    if (this.showCustomize_ && this.showWallpaperSearch_) {
      this.selectedCustomizeDialogPage_ = null;
      this.setCustomizeChromeSidePanelVisible_(!this.showCustomize_);
      return;
    }

    // Open Wallpaper Search if the side panel is closed. Otherwise, navigate
    // the side panel to Wallpaper Search.
    this.selectedCustomizeDialogPage_ = CustomizeDialogPage.WALLPAPER_SEARCH;
    this.showWallpaperSearch_ = true;
    this.setCustomizeChromeSidePanelVisible_(this.showWallpaperSearch_);
    if (!this.showCustomize_) {
      this.customizeButtonsHandler_.incrementCustomizeChromeButtonOpenCount();
      recordCustomizeChromeOpen(
          NtpCustomizeChromeEntryPoint.WALLPAPER_SEARCH_BUTTON);
    }
  }

  protected onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * Handles <CTRL> + <SHIFT> + <.> (also <CMD> + <SHIFT> + <.> on mac) to open
   * voice search.
   */
  private onWindowKeydown_(e: KeyboardEvent) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (e.key === 'Escape' && this.showComposebox_) {
      const composebox =
          this.shadowRoot.querySelector<ComposeboxElement>('#composebox');
      if (composebox) {
        composebox.handleEscapeKeyLogic();
        e.preventDefault();
        return;
      }
    }
    if (ctrlKeyPressed && e.code === 'Period' && e.shiftKey) {
      this.showVoiceSearchOverlay_ = true;
      recordVoiceAction(VoiceAction.ACTIVATE_KEYBOARD);
    }
  }

  private rgbaOrInherit_(skColor: SkColor|null): string {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  private computeShowBackgroundImage_(): boolean {
    return !!this.theme_ && !!this.theme_.backgroundImage;
  }

  private onShowBackgroundImageChange_() {
    this.backgroundManager_.setShowBackgroundImage(this.showBackgroundImage_);
  }

  private onThemeChange_() {
    if (this.theme_) {
      this.backgroundManager_.setBackgroundColor(this.theme_.backgroundColor);
      this.style.setProperty(
          '--color-new-tab-page-attribution-foreground',
          this.rgbaOrInherit_(this.theme_.textColor));
      this.style.setProperty(
          '--color-new-tab-page-most-visited-foreground',
          this.rgbaOrInherit_(this.theme_.textColor));
    }
    this.updateBackgroundImagePath_();
  }

  private onThemeLoaded_(theme: Theme) {
    recordSparseValueWithPersistentHash(
        'NewTabPage.Collections.IdOnLoad',
        theme.backgroundImageCollectionId ?? '');

    if (!theme.backgroundImage) {
      recordEnumeration(
          'NewTabPage.BackgroundImageSource', NtpBackgroundImageSource.kNoImage,
          NtpBackgroundImageSource.MAX_VALUE + 1);
    } else {
      recordEnumeration(
          'NewTabPage.BackgroundImageSource', theme.backgroundImage.imageSource,
          NtpBackgroundImageSource.MAX_VALUE + 1);
    }
  }

  private onPromoAndModulesLoadedChange_() {
    if (this.promoAndModulesLoaded_ &&
        loadTimeData.getBoolean('modulesEnabled')) {
      recordLoadDuration(
          'NewTabPage.Modules.ShownTime', WindowProxy.getInstance().now());
    }
  }

  /**
   * Set the #backgroundImage |path| only when different and non-empty. Reset
   * the customize dialog background selection if the dialog is closed.
   *
   * The ntp-untrusted-iframe |path| is set directly. When using a data binding
   * instead, the quick updates to the |path| result in iframe loading an error
   * page.
   */
  private updateBackgroundImagePath_() {
    const backgroundImage = this.theme_ && this.theme_.backgroundImage;
    if (!backgroundImage) {
      return;
    }

    this.backgroundManager_.setBackgroundImage(backgroundImage);

    if (this.wallpaperSearchButtonAnimationEnabled_ &&
            backgroundImage.imageSource ===
                NtpBackgroundImageSource.kWallpaperSearch ||
        backgroundImage.imageSource ===
            NtpBackgroundImageSource.kWallpaperSearchInspiration) {
      this.wallpaperSearchButtonAnimationEnabled_ = false;
    }
  }

  private computeColorSourceIsBaseline(): boolean {
    return !!this.theme_ && this.theme_.isBaseline;
  }

  private computeLogoColor_(): SkColor|null {
    if (!this.theme_) {
      return null;
    }

    return this.theme_.logoColor ||
        (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null);
  }

  private computeSingleColoredLogo_(): boolean {
    return !!this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
  }

  /**
   * Sends the command received from the given source and origin to the browser.
   * Relays the browser response to whether or not a promo containing the given
   * command can be shown back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the response back to the source promo
   * frame and should not be used for anything else.
   * @param  messageData Data received from the source promo frame.
   * @param commandSource Source promo frame.
   * @param commandOrigin Origin of the source promo frame.
   */
  private canShowPromoWithBrowserCommand_(
      messageData: CanShowPromoWithBrowserCommandData, commandSource: Window,
      commandOrigin: string) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!Command} */
    const commandId = Object.values(Command).includes(messageData.commandId) ?
        messageData.commandId :
        Command.kUnknownCommand;

    BrowserCommandProxy.getInstance().handler.canExecuteCommand(commandId).then(
        ({canExecute}) => {
          const response = {
            messageType: messageData.messageType,
            [messageData.commandId]: canExecute,
          };
          commandSource.postMessage(response, commandOrigin);
        });
  }

  /**
   * Sends the command and the accompanying mouse click info received from the
   * promo of the given source and origin to the browser. Relays the execution
   * status response back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the execution status response back to
   * the source promo frame and should not be used for anything else.
   * @param commandData Command and mouse click info.
   * @param commandSource Source promo frame.
   * @param commandOrigin Origin of the source promo frame.
   */
  private executePromoBrowserCommand_(
      commandData: ExecutePromoBrowserCommandData, commandSource: Window,
      commandOrigin: string) {
    // Make sure we don't send unsupported commands to the browser.
    const commandId = Object.values(Command).includes(commandData.commandId) ?
        commandData.commandId :
        Command.kUnknownCommand;

    BrowserCommandProxy.getInstance()
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
   */
  private handleOneGoogleBarMessage_(event: MessageEvent) {
    const data = event.data;
    if (data.messageType === 'loaded') {
      const oneGoogleBar = $$<IframeElement>(this, '#oneGoogleBar')!;
      oneGoogleBar.style.clipPath = 'url(#oneGoogleBarClipPath)';
      oneGoogleBar.style.zIndex = '1000';
      this.oneGoogleBarLoaded_ = true;
      this.pageHandler_.onOneGoogleBarRendered(WindowProxy.getInstance().now());
    } else if (data.messageType === 'overlaysUpdated') {
      this.$.oneGoogleBarClipPath.querySelectorAll('rect').forEach(el => {
        el.remove();
      });
      const overlayRects = data.data as DOMRect[];
      overlayRects.forEach(({x, y, width, height}) => {
        const rectElement =
            document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        // Add 8px around every rect to ensure shadows are not cutoff.
        rectElement.setAttribute('x', `${x - 8}`);
        rectElement.setAttribute('y', `${y - 8}`);
        rectElement.setAttribute('width', `${width + 16}`);
        rectElement.setAttribute('height', `${height + 16}`);
        this.$.oneGoogleBarClipPath.appendChild(rectElement);
      });
    } else if (data.messageType === 'can-show-promo-with-browser-command') {
      this.canShowPromoWithBrowserCommand_(
          data, event.source as Window, event.origin);
    } else if (data.messageType === 'execute-browser-command') {
      this.executePromoBrowserCommand_(
          data.data, event.source as Window, event.origin);
    } else if (data.messageType === 'click') {
      recordClick(NtpElement.ONE_GOOGLE_BAR);
    }
  }

  protected onMiddleSlotPromoLoaded_() {
    this.middleSlotPromoLoaded_ = true;
  }

  protected onModulesLoaded_(e: CustomEvent<number|null>) {
    this.modulesLoadedStatus_ = e.detail ?
        ModuleLoadStatus.MODULE_LOAD_COMPLETE :
        ModuleLoadStatus.MODULE_LOAD_NOT_ATTEMPTED;
  }

  protected computeShowBrowserPromo_(): boolean {
    return !this.modulesEnabled_ ||
        (this.modulesLoadedStatus_ !==
             ModuleLoadStatus.MODULE_LOAD_IN_PROGRESS &&
         !this.modulesShownToUser);
  }

  protected recordBrowserPromoMetrics_() {
    if (!this.showBrowserPromo_) {
      recordShowBrowserPromosResult(ShowNtpPromosResult.kNotShownDueToPolicy);
      return;
    }

    switch (this.browserPromoType_) {
      case 'disabled':
        recordShowBrowserPromosResult(ShowNtpPromosResult.kNotShownDueToPolicy);
        break;
      case 'empty':
        recordShowBrowserPromosResult(ShowNtpPromosResult.kNotShownNoPromos);
        break;
      case 'simple':
      case 'setuplist':
        recordShowBrowserPromosResult(ShowNtpPromosResult.kShown);
        break;
      default:
        break;
    }
  }

  protected onCustomizeModule_() {
    this.showCustomize_ = true;
    this.selectedCustomizeDialogPage_ = CustomizeDialogPage.MODULES;
    recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.MODULE);
    this.setCustomizeChromeSidePanelVisible_(this.showCustomize_);
  }

  private setCustomizeChromeSidePanelVisible_(visible: boolean) {
    let section: CustomizeChromeSection = CustomizeChromeSection.kUnspecified;
    switch (this.selectedCustomizeDialogPage_) {
      case CustomizeDialogPage.BACKGROUNDS:
      case CustomizeDialogPage.THEMES:
        section = CustomizeChromeSection.kAppearance;
        break;
      case CustomizeDialogPage.SHORTCUTS:
        section = CustomizeChromeSection.kShortcuts;
        break;
      case CustomizeDialogPage.MODULES:
        section = CustomizeChromeSection.kModules;
        break;
      case CustomizeDialogPage.WALLPAPER_SEARCH:
        section = CustomizeChromeSection.kWallpaperSearch;
        break;
    }
    this.customizeButtonsHandler_.setCustomizeChromeSidePanelVisible(
        visible, section, SidePanelOpenTrigger.kNewTabPage);
  }

  private printPerformanceDatum_(
      name: string, time: number, auxTime: number = 0) {
    if (!this.shouldPrintPerformance_) {
      return;
    }

    console.info(
        !auxTime ? `${name}: ${time}` : `${name}: ${time} (${auxTime})`);
  }

  /**
   * Prints performance measurements to the console. Also, installs  performance
   * observer to continuously print performance measurements after.
   */
  private printPerformance_() {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    const entryTypes = ['paint', 'measure'];
    const log = (entry: PerformanceEntry) => {
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

  protected onWebstoreToastButtonClick_() {
    window.location.assign(
        `https://chrome.google.com/webstore/category/collection/chrome_color_themes?hl=${
            window.navigator.language}`);
  }

  private onWindowClick_(e: Event) {
    if (e.composedPath() && e.composedPath()[0] === $$(this, '#content')) {
      recordClick(NtpElement.BACKGROUND);
      return;
    }
    for (const target of e.composedPath()) {
      switch (target) {
        case $$(this, 'ntp-logo'):
          recordClick(NtpElement.LOGO);
          return;
        case $$(this, 'cr-searchbox'):
          recordClick(NtpElement.REALBOX);
          return;
        case $$(this, 'ntp-action-chips'):
          recordClick(NtpElement.ACTION_CHIPS);
          return;
        case $$(this, 'cr-most-visited'):
          recordClick(NtpElement.MOST_VISITED);
          return;
        case $$(this, 'ntp-middle-slot-promo'):
          recordClick(NtpElement.MIDDLE_SLOT_PROMO);
          return;
        case $$(this, '#modules'):
          recordClick(NtpElement.MODULE);
          return;
      }
    }

    const customizeButtonsElement =
        this.shadowRoot.querySelector('ntp-customize-buttons');
    if (customizeButtonsElement) {
      for (const target of e.composedPath()) {
        switch (target) {
          case $$(customizeButtonsElement, '#customizeButton'):
            recordClick(NtpElement.CUSTOMIZE_BUTTON);
            return;
          case $$(customizeButtonsElement, '#wallpaperSearchButton'):
            recordClick(NtpElement.WALLPAPER_SEARCH_BUTTON);
            return;
        }
      }
    }

    recordClick(NtpElement.OTHER);
  }

  protected isThemeDark_(): boolean {
    return !!this.theme_ && this.theme_.isDark;
  }

  protected showThemeAttribution_(): boolean {
    return !!this.theme_?.backgroundImage?.attributionUrl;
  }

  protected onInputFocusChanged_(e: CustomEvent<{value: boolean}>) {
    switch (e.type) {
      case 'searchbox-input-focus-changed':
        this.searchboxInputFocused_ = e.detail.value;
        break;
      case 'composebox-input-focus-changed':
        this.composeboxInputFocused_ = e.detail.value;
        break;
    }
  }

  protected onRealboxHadSecondarySideChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.realboxHadSecondarySide = e.detail.value;
  }

  protected onModulesShownToUserChanged_(e: CustomEvent<{value: boolean}>) {
    this.modulesShownToUser = e.detail.value;
  }

  protected onActionChipsRetrievalStateChanged_(
      e: CustomEvent<{state: ActionChipsRetrievalState}>) {
    const state = e.detail.state;
    // Mapping of ActionChipsRetrievalState => GlifAnimationState:
    // REQUESTED => SPINNER_ONLY
    // UPDATED => STARTED (or FINISHED if cr_context_menu_entrypoint sets it)
    // To avoid going back (or continuing) GlifAnimationState.STARTED, we stop
    // updating the field when the current state is STARTED or FINISHED.
    // There are a few cases to consider:
    // - IsActionChipsVisible_ is false (and remains so): no event from the
    //   action chips element, and thus the animation state remains INELIGIBLE.
    // - IsActionChipsVisible_ is false and later becomes true: the change
    //   triggers the rendering of the action chips element, and this in turn
    //   causes an event with ActionChipsRetrievalState.REQUESTED to be fired.
    //   After some time, an event with ActionChipsRetrievalState.UPDATED will
    //   fire, and this starts the animation.
    // - IsActionChipsVisible_ is true from the beginning: Same as above.
    if ([GlifAnimationState.STARTED, GlifAnimationState.FINISHED].every(
            s => s !== this.contextMenuGlifAnimationState_)) {
      if (state === ActionChipsRetrievalState.REQUESTED) {
        this.contextMenuGlifAnimationState_ = GlifAnimationState.SPINNER_ONLY;
      } else if (state === ActionChipsRetrievalState.UPDATED) {
        this.contextMenuGlifAnimationState_ = GlifAnimationState.STARTED;
      }
    }
  }

  /**
   * Called whenever an auto-removed feature is being processed and the undo
   * toast needs to be shown. This will queue up the toast in the pending FIFO
   * list and then call the processing function.
   *
   * @param undoToastContext - An event that contains the undo toast message and
   *                           the undo callback function.
   */
  protected showAutoRemovedToast_(
      undoToastContext: CustomEvent<{message: string, undo: () => void}>) {
    this.pendingAutoRemovalToasts_.push(undoToastContext.detail);
    this.processPendingAutoRemovalToasts_();
  }

  /**
   * Called whenever the pending toasts need to be processed. This is called
   * whenever a new toast is added to the pending list through an auto-removal
   * event, or when the user clicks on the undo button in the toast.
   *
   * In case the undo toast is already open, then it's a no-op to avoid showing
   * multiple toasts at the same time. Otherwise, the first pending toast is
   * popped and shown.
   */
  private processPendingAutoRemovalToasts_() {
    if (this.pendingAutoRemovalToasts_.length === 0) {
      return;
    }

    if (this.$.undoToast.open) {
      return;
    }

    const undoToastContext = this.pendingAutoRemovalToasts_.shift()!;
    this.undoAutoRemovalCallback_ = undoToastContext.undo;
    this.undoAutoRemovalMessage_ = undoToastContext.message;
    this.$.undoToast.show();
  }

  /**
   * Processes an auto-removal undo click. It will hide the toast, call the
   * undo callback, and call the processing function to handle the next queued
   * toast (if any).
   */
  protected onAutoRemovalUndoClick_() {
    this.$.undoToast.hide();
    this.undoAutoRemovalCallback_?.();
    this.undoAutoRemovalCallback_ = null;
    this.undoAutoRemovalMessage_ = null;
    this.processPendingAutoRemovalToasts_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
