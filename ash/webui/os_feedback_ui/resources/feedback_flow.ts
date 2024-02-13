// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './confirmation_page.js';
import './search_page.js';
import './share_data_page.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConfirmationPageElement} from './confirmation_page.js';
import {getTemplate} from './feedback_flow.html.js';
import {showScrollingEffectOnStart, showScrollingEffects} from './feedback_utils.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';
import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './os_feedback_ui.mojom-webui.js';
import {SearchPageElement} from './search_page.js';
import {ShareDataPageElement} from './share_data_page.js';

/**  The host of untrusted child page. */
const OS_FEEDBACK_UNTRUSTED_ORIGIN = 'chrome-untrusted://os-feedback';

/**  The id of help-content-clicked message. */
const HELP_CONTENT_CLICKED = 'help-content-clicked';

export type FeedbackFlowButtonClickEvent = CustomEvent<{
  currentState: FeedbackFlowState,
  description?: string,
  report?: Report,
}>;

/**  Enum for actions on search page. */
export enum SearchPageAction {
  CONTINUE = 'continue',
  QUIT = 'quit',
}

/**  Enum for feedback flow states. */
export enum FeedbackFlowState {
  SEARCH = 'searchPage',
  SHARE_DATA = 'shareDataPage',
  CONFIRMATION = 'confirmationPage'
}

/**
 * Enum for reserved query parameters used by feedback source to provide
 * addition context to final report.
 */
export const AdditionalContextQueryParam = {
  DESCRIPTION_TEMPLATE: 'description_template',
  DESCRIPTION_PLACEHOLDER_TEXT: 'description_placeholder_text',
  EXTRA_DIAGNOSTICS: 'extra_diagnostics',
  CATEGORY_TAG: 'category_tag',
  PAGE_URL: 'page_url',
  FROM_ASSISTANT: 'from_assistant',
  FROM_SETTINGS_SEARCH: 'from_settings_search',
  FROM_AUTOFILL: 'from_autofill',
  AUTOFILL_METADATA: 'autofill_metadata',
};

/**
 * Builds a RegExp that matches one of the given words. Each word has to match
 * at word boundary and is not at the end of the tested string. For example,
 * the word "SIM" would match the string "I have a sim card issue" but not
 * "I have a simple issue" nor "I have a sim" (because the user might not have
 * finished typing yet).
 */
export function buildWordMatcher(words: string[]): RegExp {
  return new RegExp(
      words.map((word) => '\\b' + word + '\\b[^$]').join('|'), 'i');
}

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or
 * without space between the words; for BT when used as an individual word,
 * or as two individual characters, and for BLE, BlueZ, and Floss when used
 * as an individual word. Case insensitive matching.
 */
export const btRegEx = new RegExp(
    'blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b|\\bfloss\\b|\\bbluez\\b', 'i');

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 */
const cantConnectRegEx = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether", "tethering" or "hotspot". Case
 * insensitive matching.
 */
const tetherRegEx = new RegExp('tether(ing)?|hotspot', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with
 * or without space between the words. Case insensitive matching.
 */
const smartLockRegEx = new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 */
const nearbyShareRegEx = new RegExp('nearby|phone', 'i');

/**
 * Regular expression to check for keywords related to Fast Pair like
 * "fast pair".
 * Case insensitive matching.
 */
const fastPairRegEx = new RegExp('fast[ ]?pair', 'i');

/**  Regular expression to check for Bluetooth device specific keywords. */
const btDeviceRegEx =
    buildWordMatcher(['apple', 'allegro', 'pixelbud', 'microsoft', 'sony']);

/**
 * Regular expression to check for Phone Hub / Eche device specific keywords
 * like "app stream" or "camera roll".
 */
const phoneHubRegEx =
    new RegExp('app[ ]?stream(ing)?|phone|camera[ ]?roll', 'i');

/**  Regular expression to check for wifi-related keywords. */
const wifiRegEx =
    new RegExp('\\b(wifi|wi\-fi|internet|network|hotspot)\\b', 'i');

/**
 * @fileoverview
 * 'feedback-flow' manages the navigation among the steps to be taken.
 */

export class FeedbackFlowElement extends PolymerElement {
  static get is() {
    return 'feedback-flow' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentState: {type: String},
      feedbackContext: {type: Object, readonly: false, notify: true},
    };
  }

  /**  The id of an element on the page that is currently shown. */
  protected currentState: FeedbackFlowState = FeedbackFlowState.SEARCH;

  /**  The feedback context. */
  protected feedbackContext: FeedbackContext|null;

  /**  Whether to show the bluetooth Logs checkbox in share data page. */
  shouldShowBluetoothCheckbox: boolean = false;

  /**  Whether to show the Wifi debug Logs checkbox in share data page. */
  shouldShowWifiDebugLogsCheckbox = false;

  /**
   * Whether to show the Link Cross Device Dogfood Feedback checkbox in share
   * data page.
   */
  shouldShowLinkCrossDeviceDogfoodFeedbackCheckbox = false;

  /**  Whether to show the autofill checkbox in share data page. */
  protected shouldShowAutofillCheckbox = false;

  /**  Whether to show the assistant checkbox in share data page. */
  shouldShowAssistantCheckbox = false;

  private feedbackServiceProvider: FeedbackServiceProviderInterface;

  /**
   * The description entered by the user. It is set when the user clicks the
   * next button on the search page.
   */
  private description: string;

  /**
   * The description template provided source application to help user write
   * feedback.
   */
  protected descriptionTemplate: string;

  /**
   * The description placeholder text is used to give the user a hint on how
   * to write the description. Some apps, such as the Camera app can use a
   * custom placeholder.
   */
  protected descriptionPlaceholderText: string;

  /**  The status of sending report. */
  private sendReportStatus: SendReportStatus|null;

  /**  Whether user clicks the help content or not. */
  private helpContentClicked = false;

  /**  To avoid helpContentOutcome metric emit more than one time. */
  private helpContentOutcomeMetricEmitted = false;

  /**  Number of results returned in each search. */
  private helpContentSearchResultCount: number;

  /**  Whether there is no help content shown(offline or search is down). */
  private noHelpContentDisplayed: boolean;

  /**
   * When the feedback tool is opened as a dialog, feedback context is passed
   * to front end via dialog args.
   */
  private dialogArgs: string;

  /**  Whether the user has logged in (not on oobe or on the login screen). */
  private isUserLoggedIn: boolean;

  constructor() {
    super();
    this.dialogArgs = chrome.getVariableValue('dialogArguments');
    this.feedbackServiceProvider = getFeedbackServiceProvider();
  }

  override connectedCallback() {
    super.connectedCallback();
    // TODO(b/276493287): After the Jelly experiment is launched, replace
    // `cros_styles.css` with `theme/colors.css` directly in `index.html`.
    // Also add `theme/typography.css` to `index.html`.
    document.querySelector('link[href*=\'cros_styles.css\']')
        ?.setAttribute('href', 'chrome://theme/colors.css?sets=legacy,sys');
    const typographyLink = document.createElement('link');
    typographyLink.href = 'chrome://theme/typography.css';
    typographyLink.rel = 'stylesheet';
    document.head.appendChild(typographyLink);
    document.body.classList.add('jelly-enabled');
    /** @suppress {checkTypes} */
    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  }

  override ready() {
    super.ready();
    if (this.dialogArgs && this.dialogArgs.length > 0) {
      this.initializeForDialogMode();
    } else {
      this.initializeForNonDialogMode();
    }

    window.addEventListener('message', event => {
      if (event.data.id !== HELP_CONTENT_CLICKED) {
        return;
      }
      if (event.origin !== OS_FEEDBACK_UNTRUSTED_ORIGIN) {
        console.error('Unknown origin: ' + event.origin);
        return;
      }
      this.helpContentClicked = true;
      this.feedbackServiceProvider.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedHelpContent);
      this.feedbackServiceProvider.recordHelpContentSearchResultCount(
          this.helpContentSearchResultCount);
    });

    window.addEventListener('beforeunload', _event => {
      switch (this.currentState) {
        case FeedbackFlowState.SEARCH:
          this.recordExitPath(
              FeedbackAppExitPath.kQuitSearchPageHelpContentClicked,
              FeedbackAppExitPath.kQuitSearchPageNoHelpContentClicked);
          if (!this.helpContentOutcomeMetricEmitted) {
            this.recordHelpContentOutcome(
                SearchPageAction.QUIT,
                FeedbackAppHelpContentOutcome.kQuitHelpContentClicked,
                FeedbackAppHelpContentOutcome.kQuitNoHelpContentClicked);
            this.helpContentOutcomeMetricEmitted = true;
          }
          break;
        case FeedbackFlowState.SHARE_DATA:
          this.recordExitPath(
              FeedbackAppExitPath.kQuitShareDataPageHelpContentClicked,
              FeedbackAppExitPath.kQuitShareDataPageNoHelpContentClicked);
          break;
        case FeedbackFlowState.CONFIRMATION:
          this.recordExitPath(
              FeedbackAppExitPath.kSuccessHelpContentClicked,
              FeedbackAppExitPath.kSuccessNoHelpContentClicked);
          break;
      }
    });

    this.style.setProperty(
        '--window-height', window.innerHeight.toString() + 'px');
    window.addEventListener('resize', (event) => {
      this.style.setProperty(
          '--window-height', window.innerHeight.toString() + 'px');
      let page = null;
      switch (this.currentState) {
        case FeedbackFlowState.SEARCH:
          page = strictQuery('search-page', this.shadowRoot, SearchPageElement);
          break;
        case FeedbackFlowState.SHARE_DATA:
          page = strictQuery(
              'share-data-page', this.shadowRoot, ShareDataPageElement);
          break;
        case FeedbackFlowState.CONFIRMATION:
          page = strictQuery(
              'confirmation-page', this.shadowRoot, ConfirmationPageElement);
          break;
        default:
          console.warn('unexpected state: ', this.currentState);
      }
      if (page) {
        showScrollingEffects(event, page as HTMLElement);
      }
    });
  }

  /** TODO(http://b/issues/233080620): Add a type definition for feedbackInfo.*/
  private initializeForDialogMode() {
    // This is on Dialog mode. The `dialogArgs` contains feedback context
    // info.
    const feedbackInfo = JSON.parse(this.dialogArgs);
    assert(!!feedbackInfo);
    this.feedbackContext = {
      assistantDebugInfoAllowed: false,
      fromSettingsSearch: feedbackInfo.fromSettingsSearch ?? false,
      isInternalAccount: feedbackInfo.isInternalAccount ?? false,
      wifiDebugLogsAllowed: false,
      traceId: feedbackInfo.traceId ?? 0,
      pageUrl: {url: feedbackInfo.pageUrl ?? ''},
      fromAssistant: feedbackInfo.fromAssistant ?? false,
      fromAutofill: feedbackInfo.fromAutofill ?? false,
      autofillMetadata: feedbackInfo.autofillMetadata ?
          JSON.stringify(feedbackInfo.autofillMetadata) :
          '{}',
      hasLinkedCrossDevicePhone:
          feedbackInfo.hasLinkedCrossDevicePhone ?? false,
      categoryTag: feedbackInfo.categoryTag ?? '',
      email: '',
      extraDiagnostics: '',
    };
    this.descriptionTemplate = feedbackInfo.description ?? '';
    this.descriptionPlaceholderText = feedbackInfo.descriptionPlaceholder ?? '';

    if (feedbackInfo.systemInformation?.length == 1) {
      // Currently, one extra diagnostics string may be passed to feedback
      // app.
      //
      // Sample input:
      //" systemInformation": [
      //   {
      //    "key": "EXTRA_DIAGNOSTICS",
      //    "value": "extra log data"
      //   }
      // ].
      assert('EXTRA_DIAGNOSTICS' === feedbackInfo.systemInformation[0].key);
      this.feedbackContext!.extraDiagnostics =
          feedbackInfo.systemInformation[0].value;
    }
    this.isUserLoggedIn = this.feedbackContext!.categoryTag !== 'Login';
    this.onFeedbackContextReceived();
  }


  private initializeForNonDialogMode(): void {
    this.feedbackServiceProvider.getFeedbackContext().then(
        (response: {feedbackContext: FeedbackContext}) => {
          this.feedbackContext = response.feedbackContext;
          this.isUserLoggedIn = true;
          this.setAdditionalContextFromQueryParams();
          this.onFeedbackContextReceived();
        });
  }

  private fetchScreenshot(): void {
    const shareDataPage =
        strictQuery('share-data-page', this.shadowRoot, ShareDataPageElement);
    // Fetch screenshot if not fetched before.
    if (!shareDataPage.screenshotUrl) {
      this.feedbackServiceProvider.getScreenshotPng().then(
          (response: {pngData: number[]}) => {
            if (response.pngData.length > 0) {
              const blob = new Blob(
                  [Uint8Array.from(response.pngData)], {type: 'image/png'});
              const imageUrl = URL.createObjectURL(blob);
              shareDataPage.screenshotUrl = imageUrl;
            }
          });
    }
  }

  private onFeedbackContextReceived(): void {
    if (!this.feedbackContext) {
      return;
    }
    this.shouldShowAssistantCheckbox = this.feedbackContext.isInternalAccount &&
        this.feedbackContext.fromAssistant;
    this.shouldShowAutofillCheckbox = this.feedbackContext.fromAutofill;
  }
  /**
   * Sets additional context passed from RequestFeedbackFlow as part of the URL.
   * See `AdditionalContextQueryParam` for valid query parameters.
   */
  private setAdditionalContextFromQueryParams(): void {
    if (!this.feedbackContext) {
      return;
    }

    const params = new URLSearchParams(window.location.search);
    const extraDiagnostics =
        params.get(AdditionalContextQueryParam.EXTRA_DIAGNOSTICS);
    this.feedbackContext.extraDiagnostics =
        extraDiagnostics ? decodeURIComponent(extraDiagnostics) : '';
    const descriptionTemplate =
        params.get(AdditionalContextQueryParam.DESCRIPTION_TEMPLATE);
    this.descriptionTemplate =
        descriptionTemplate && descriptionTemplate.length > 0 ?
        decodeURIComponent(descriptionTemplate) :
        '';
    const descriptionPlaceholderText =
        params.get(AdditionalContextQueryParam.DESCRIPTION_PLACEHOLDER_TEXT);
    this.descriptionPlaceholderText =
        descriptionPlaceholderText && descriptionPlaceholderText.length > 0 ?
        decodeURIComponent(descriptionPlaceholderText) :
        '';
    const categoryTag = params.get(AdditionalContextQueryParam.CATEGORY_TAG);
    this.feedbackContext.categoryTag =
        categoryTag ? decodeURIComponent(categoryTag) : '';
    const pageUrl = params.get(AdditionalContextQueryParam.PAGE_URL);
    if (pageUrl) {
      this.set('feedbackContext.pageUrl', {url: pageUrl});
    }
    const fromAssistant =
        params.get(AdditionalContextQueryParam.FROM_ASSISTANT);
    this.feedbackContext.fromAssistant = !!fromAssistant;
    const fromSettingsSearch =
        params.get(AdditionalContextQueryParam.FROM_SETTINGS_SEARCH);
    this.set('feedbackContext.fromSettingsSearch', !!fromSettingsSearch);

    const fromAutofill = params.get(AdditionalContextQueryParam.FROM_AUTOFILL);
    this.feedbackContext.fromAutofill = !!fromAutofill;
    const autofillMetadata =
        params.get(AdditionalContextQueryParam.AUTOFILL_METADATA);
    if (autofillMetadata) {
      this.feedbackContext.autofillMetadata = autofillMetadata;
    }
  }

  protected handleContinueClick(event: Event): void {
    const customEvent = event as FeedbackFlowButtonClickEvent;
    switch (customEvent.detail.currentState) {
      case FeedbackFlowState.SEARCH:
        this.currentState = FeedbackFlowState.SHARE_DATA;
        this.description = customEvent.detail.description ?? '';
        this.shouldShowBluetoothCheckbox = this.feedbackContext !== null &&
            this.feedbackContext.isInternalAccount &&
            this.isDescriptionRelatedToBluetooth(this.description);
        this.shouldShowWifiDebugLogsCheckbox =
            this.computeShouldShowWifiDebugLogsCheckbox();
        this.shouldShowLinkCrossDeviceDogfoodFeedbackCheckbox =
            this.feedbackContext !== null &&
            loadTimeData.getBoolean(
                'enableLinkCrossDeviceDogfoodFeedbackFlag') &&
            this.feedbackContext.isInternalAccount &&
            this.feedbackContext.hasLinkedCrossDevicePhone &&
            this.isDescriptionRelatedToCrossDevice(this.description);
        this.fetchScreenshot();
        const shareDataPage = strictQuery(
            'share-data-page', this.shadowRoot, ShareDataPageElement);
        shareDataPage.focusScreenshotCheckbox();
        showScrollingEffectOnStart(shareDataPage);

        if (!this.helpContentOutcomeMetricEmitted) {
          this.recordHelpContentOutcome(
              SearchPageAction.CONTINUE,
              FeedbackAppHelpContentOutcome.kContinueHelpContentClicked,
              FeedbackAppHelpContentOutcome.kContinueNoHelpContentClicked);
          this.helpContentOutcomeMetricEmitted = true;
        }
        break;
      case FeedbackFlowState.SHARE_DATA:
        const report = customEvent.detail.report as Report;
        report.description = stringToMojoString16(this.description);

        // TODO(xiangdongkong): Show a spinner or the like for sendReport could
        // take a while.
        this.feedbackServiceProvider.sendReport(report).then(
            (response: {status: SendReportStatus}) => {
              this.currentState = FeedbackFlowState.CONFIRMATION;
              this.sendReportStatus = response.status;
              const confirmationPage = strictQuery(
                  'confirmation-page', this.shadowRoot,
                  ConfirmationPageElement);
              confirmationPage.focusPageTitle();
              showScrollingEffectOnStart(confirmationPage);
            });
        break;
      default:
        console.warn('unexpected state: ', customEvent.detail.currentState);
    }
  }

  protected handleGoBackClick(event: Event): void {
    const customEvent = event as FeedbackFlowButtonClickEvent;
    switch (customEvent.detail.currentState) {
      case FeedbackFlowState.SHARE_DATA:
        this.navigateToSearchPage();
        break;
      case FeedbackFlowState.CONFIRMATION:
        // Remove the text from previous search.
        const searchPage =
            strictQuery('search-page', this.shadowRoot, SearchPageElement);
        searchPage.setDescription(/*text=*/ '');
        showScrollingEffectOnStart(searchPage);

        // Re-enable the send button in share data page.
        const shareDataPage = strictQuery(
            'share-data-page', this.shadowRoot, ShareDataPageElement);
        shareDataPage.reEnableSendReportButton();

        // Re-enable helpContentOutcomeMetric to be emitted in search page.
        this.helpContentOutcomeMetricEmitted = false;

        this.navigateToSearchPage();
        break;
      default:
        console.warn('unexpected state: ', customEvent.detail.currentState);
    }
  }

  private computeShouldShowWifiDebugLogsCheckbox(): boolean {
    return this.feedbackContext !== null &&
        this.feedbackContext.isInternalAccount &&
        this.feedbackContext.wifiDebugLogsAllowed &&
        wifiRegEx.test(this.description);
  }

  private navigateToSearchPage(): void {
    this.currentState = FeedbackFlowState.SEARCH;
    const searchPage =
        strictQuery('search-page', this.shadowRoot, SearchPageElement);
    searchPage.focusInputElement();
    showScrollingEffectOnStart(searchPage);
  }

  private recordHelpContentOutcome(
      action: SearchPageAction,
      outcomeHelpContentClicked: FeedbackAppHelpContentOutcome,
      outcomeNoHelpContentClicked: FeedbackAppHelpContentOutcome): void {
    if (this.noHelpContentDisplayed) {
      action == SearchPageAction.CONTINUE ?
          this.feedbackServiceProvider.recordHelpContentOutcome(
              FeedbackAppHelpContentOutcome.kContinueNoHelpContentDisplayed) :
          this.feedbackServiceProvider.recordHelpContentOutcome(
              FeedbackAppHelpContentOutcome.kQuitNoHelpContentDisplayed);
      return;
    }

    this.helpContentClicked ?
        this.feedbackServiceProvider.recordHelpContentOutcome(
            outcomeHelpContentClicked) :
        this.feedbackServiceProvider.recordHelpContentOutcome(
            outcomeNoHelpContentClicked);
  }

  private recordExitPath(
      pathHelpContentClicked: FeedbackAppExitPath,
      pathNoHelpContentClicked: FeedbackAppExitPath) {
    if (this.noHelpContentDisplayed) {
      this.feedbackServiceProvider.recordExitPath(
          FeedbackAppExitPath.kQuitNoHelpContentDisplayed);
      return;
    }
    this.helpContentClicked ?
        this.feedbackServiceProvider.recordExitPath(pathHelpContentClicked) :
        this.feedbackServiceProvider.recordExitPath(pathNoHelpContentClicked);
  }


  setCurrentStateForTesting(newState: FeedbackFlowState) {
    this.currentState = newState;
  }

  setSendReportStatusForTesting(status: SendReportStatus) {
    this.sendReportStatus = status;
  }

  setDescriptionForTesting(text: string) {
    this.description = text;
  }

  setHelpContentClickedForTesting(helpContentClicked: boolean) {
    this.helpContentClicked = helpContentClicked;
  }

  setNoHelpContentDisplayedForTesting(noHelpContentDisplayed: boolean) {
    this.noHelpContentDisplayed = noHelpContentDisplayed;
  }

  getDescriptionTemplateForTesting(): string|null {
    return this.descriptionTemplate;
  }

  getDescriptionPlaceholderTextForTesting(): string|null {
    return this.descriptionPlaceholderText;
  }

  getIsUserLoggedInForTesting(): boolean {
    return this.isUserLoggedIn;
  }

  getFeedbackContextForTesting(): FeedbackContext|null {
    return this.feedbackContext;
  }

  getShouldShowWifiDebugLogsCheckboxForTesting(): boolean {
    return this.shouldShowWifiDebugLogsCheckbox;
  }

  /**
   * Checks if any keywords related to bluetooth have been typed. If they are,
   * we show the bluetooth logs option, otherwise hide it.
   */
  protected isDescriptionRelatedToBluetooth(textInput: string): boolean {
    /**
     * If the user is not signed in with a internal google account, the
     * bluetooth checkbox should be hidden and skip the relative check.
     */
    const isRelatedToBluetooth = btRegEx.test(textInput) ||
        cantConnectRegEx.test(textInput) ||
        this.isDescriptionRelatedToCrossDevice(textInput) ||
        fastPairRegEx.test(textInput) || btDeviceRegEx.test(textInput);
    return isRelatedToBluetooth;
  }

  /**
   * If the user is not signed in with a internal google account, the Cross
   * Device checkbox should be hidden and skip the relative check.
   *
   * Checks if any keywords related to Cross Device have been typed. If they
   * are, we show the cross device checkbox, otherwise hide it.
   */
  protected isDescriptionRelatedToCrossDevice(textInput: string): boolean {
    const isRelatedToCrossDevice = phoneHubRegEx.test(textInput) ||
        tetherRegEx.test(textInput) || smartLockRegEx.test(textInput) ||
        nearbyShareRegEx.test(textInput);
    return isRelatedToCrossDevice;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FeedbackFlowElement.is]: FeedbackFlowElement;
  }
}

customElements.define(FeedbackFlowElement.is, FeedbackFlowElement);
