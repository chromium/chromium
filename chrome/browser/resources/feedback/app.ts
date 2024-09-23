// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';
import './feedback_shared_styles.css.js';
// <if expr="chromeos_ash">
import './js/jelly_colors.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {FeedbackBrowserProxyImpl} from './js/feedback_browser_proxy.js';
import {BT_DEVICE_REGEX, BT_REGEX, CANNOT_CONNECT_REGEX, CELLULAR_REGEX, DISPLAY_REGEX, FAST_PAIR_REGEX, NEARBY_SHARE_REGEX, SMART_LOCK_REGEX, TETHER_REGEX, THUNDERBOLT_REGEX, USB_REGEX, WIFI_REGEX} from './js/feedback_regexes.js';
import {FEEDBACK_LANDING_PAGE, FEEDBACK_LANDING_PAGE_TECHSTOP, FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERM_OF_SERVICE_URL, openUrlInAppWindow} from './js/feedback_util.js';
import {domainQuestions, questionnaireBegin, questionnaireNotification} from './js/questionnaire.js';
import {takeScreenshot} from './js/take_screenshot.js';

const MAX_ATTACH_FILE_SIZE: number = 3 * 1024 * 1024;
const MAX_SCREENSHOT_WIDTH: number = 100;

export class AppElement extends CrLitElement {
  static get is() {
    return 'feedback-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private formOpenTime: number = new Date().getTime();
  private attachedFileBlob: Blob|null = null;

  /**
   * Which questions have been appended to the issue description text area.
   */
  private appendedQuestions: {[key: string]: boolean} = {};

  /**
   * The object will be manipulated by sendReport().
   */
  private feedbackInfo: chrome.feedbackPrivate.FeedbackInfo = {
    attachedFile: undefined,
    attachedFileBlobUuid: undefined,
    autofillMetadata: '',
    categoryTag: undefined,
    description: '...',
    descriptionPlaceholder: undefined,
    email: undefined,
    flow: chrome.feedbackPrivate.FeedbackFlow.REGULAR,
    fromAutofill: false,
    includeBluetoothLogs: false,
    pageUrl: undefined,
    sendHistograms: undefined,
    systemInformation: [],
    useSystemWindowFrame: false,
    isOffensiveOrUnsafe: undefined,
    aiMetadata: undefined,
  };

  /**
   * Initializes our page.
   * Flow:
   * .) DOMContent Loaded        -> . Request feedbackInfo object
   *                                . Setup page event handlers
   * .) Feedback Object Received -> . take screenshot
   *                                . request email
   *                                . request System info
   *                                . request i18n strings
   * .) Screenshot taken         -> . Show Feedback window.
   */
  override async connectedCallback() {
    super.connectedCallback();

    // Initialize `browserProxy` only after tests had a chance to do setup
    // steps, one of which is to replace the prod proxy with a test version.
    // this.browserProxy = FeedbackBrowserProxyImpl.getInstance();

    const dialogArgs =
        FeedbackBrowserProxyImpl.getInstance().getDialogArguments();
    if (dialogArgs) {
      this.feedbackInfo = JSON.parse(dialogArgs);
    }

    await this.applyData(this.feedbackInfo);

    // Setup our event handlers.
    this.getRequiredElement('#attach-file')
        .addEventListener('change', (e: Event) => this.onFileSelected(e));
    this.getRequiredElement('#attach-file')
        .addEventListener('click', this.onOpenFileDialog.bind(this));
    this.getRequiredElement('#send-report-button').onclick =
        this.sendReport.bind(this);
    this.getRequiredElement('#cancel-button').onclick = (e: Event) =>
        this.cancel(e);
    this.getRequiredElement('#remove-attached-file').onclick =
        this.clearAttachedFile.bind(this);

    // Dispatch event used by tests.
    this.dispatchEvent(new CustomEvent('ready-for-testing'));
  }

  /**
   * Apply updates based on the received `FeedbackInfo` object.
   * @return A promise signaling that all UI updates have finished.
   */
  private applyData(feedbackInfo: chrome.feedbackPrivate.FeedbackInfo):
      Promise<void> {
    if (feedbackInfo.includeBluetoothLogs) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      this.getRequiredElement('#description-text')
          .addEventListener(
              'input', (e: Event) => this.checkForSendBluetoothLogs(e));
    }

    if (feedbackInfo.showQuestionnaire) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      this.getRequiredElement('#description-text')
          .addEventListener(
              'input', (e: Event) => this.checkForShowQuestionnaire(e));
    }

    if (this.shadowRoot!.querySelector<HTMLElement>(
            '#autofill-checkbox-container') != null &&
        feedbackInfo.flow ===
            chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL &&
        feedbackInfo.fromAutofill) {
      this.getRequiredElement('#autofill-checkbox-container').hidden = false;
    }

    this.getRequiredElement('#description-text').textContent =
        feedbackInfo.description;
    if (feedbackInfo.descriptionPlaceholder) {
      this.getRequiredElement<HTMLTextAreaElement>('#description-text')
          .placeholder = feedbackInfo.descriptionPlaceholder;
    }
    if (feedbackInfo.pageUrl) {
      this.getRequiredElement<HTMLInputElement>('#page-url-text').value =
          feedbackInfo.pageUrl;
    }

    const isAiFlow: boolean =
        feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.AI;

    if (isAiFlow) {
      this.getRequiredElement('#free-form-text').textContent =
          loadTimeData.getString('freeFormTextAi');
      this.getRequiredElement('#offensive-container').hidden = false;
      this.getRequiredElement('#log-id-container').hidden = false;
    }

    const isSeaPenFlow: boolean|undefined =
        isAiFlow && feedbackInfo.aiMetadata?.includes('from_sea_pen');

    if (isSeaPenFlow) {
      this.getRequiredElement('#log-id-container').hidden = true;
      this.getRequiredElement('#screenshot-container').hidden = true;
      this.getRequiredElement('#sys-info-container').hidden = true;
    }

    const whenScreenshotUpdated = takeScreenshot().then((screenshotCanvas) => {
      // We've taken our screenshot, show the feedback page without any
      // further delay.
      window.requestAnimationFrame(this.resizeAppWindow.bind(this));

      FeedbackBrowserProxyImpl.getInstance().showDialog();

      // Allow feedback to be sent even if the screenshot failed.
      if (!screenshotCanvas) {
        const checkbox =
            this.getRequiredElement<HTMLInputElement>('#screenshot-checkbox');
        checkbox.disabled = true;
        checkbox.checked = false;
        return Promise.resolve();
      }

      return new Promise<void>((resolve) => {
        screenshotCanvas.toBlob((blob) => {
          const image =
              this.getRequiredElement<HTMLImageElement>('#screenshot-image');
          image.src = URL.createObjectURL(blob!);
          // Only set the alt text when the src url is available, otherwise we'd
          // get a broken image picture instead. crbug.com/773985.
          image.alt = 'screenshot';
          image.classList.toggle(
              'wide-screen', image.width > MAX_SCREENSHOT_WIDTH);
          feedbackInfo.screenshot = blob!;
          resolve();
        });
      });
    });

    const whenEmailUpdated = isAiFlow ?
        Promise.resolve() :
        FeedbackBrowserProxyImpl.getInstance().getUserEmail().then((email) => {
          // Never add an empty option.
          if (!email) {
            return;
          }
          const optionElement = document.createElement('option');
          optionElement.value = email;
          optionElement.text = email;
          optionElement.selected = true;
          // Make sure the "Report anonymously" option comes last.
          this.getRequiredElement('#user-email-drop-down')
              .insertBefore(
                  optionElement,
                  this.getRequiredElement('#anonymous-user-option'));

          // Now we can unhide the user email section:
          this.getRequiredElement('#user-email').hidden = false;
          // Only show email consent checkbox when an email address exists.
          this.getRequiredElement('#consent-container').hidden = false;
        });

    // An extension called us with an attached file.
    if (feedbackInfo.attachedFile) {
      this.getRequiredElement('#attached-filename-text').textContent =
          feedbackInfo.attachedFile.name;
      this.attachedFileBlob = feedbackInfo.attachedFile.data!;
      this.getRequiredElement('#custom-file-container').hidden = false;
      this.getRequiredElement('#attach-file').hidden = true;
    }

    // No URL, file attachment for login screen feedback.
    if (feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      this.getRequiredElement('#page-url').hidden = true;
      this.getRequiredElement('#attach-file-container').hidden = true;
      this.getRequiredElement('#attach-file-note').hidden = true;
    }

    const autofillMetadataUrlElement =
        this.shadowRoot!.querySelector<HTMLElement>('#autofill-metadata-url');

    if (autofillMetadataUrlElement) {
      // Opens a new window showing the full anonymized autofill metadata.
      autofillMetadataUrlElement.onclick = (e: Event) => {
        e.preventDefault();

        FeedbackBrowserProxyImpl.getInstance().showAutofillMetadataInfo(
            feedbackInfo.autofillMetadata!);
      };

      autofillMetadataUrlElement.onauxclick = (e: Event) => {
        e.preventDefault();
      };
    }

    const sysInfoUrlElement =
        this.shadowRoot!.querySelector<HTMLElement>('#sys-info-url');
    if (sysInfoUrlElement) {
      // Opens a new window showing the full anonymized system+app
      // information.
      sysInfoUrlElement.onclick = (e: Event) => {
        e.preventDefault();

        FeedbackBrowserProxyImpl.getInstance().showSystemInfo();
      };

      sysInfoUrlElement.onauxclick = (e: Event) => {
        e.preventDefault();
      };
    }

    const histogramUrlElement =
        this.shadowRoot!.querySelector<HTMLElement>('#histograms-url');
    if (histogramUrlElement) {
      histogramUrlElement.onclick = (e: Event) => {
        e.preventDefault();

        FeedbackBrowserProxyImpl.getInstance().showMetrics();
      };

      histogramUrlElement.onauxclick = (e: Event) => {
        e.preventDefault();
      };
    }

    // The following URLs don't open on login screen, so hide them.
    // TODO(crbug.com/40144717): Find a solution to display them properly.
    // Update: the bluetooth and assistant logs links will work on login
    // screen now. But to limit the scope of this CL, they are still hidden.
    if (feedbackInfo.flow !== chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      const legalHelpPageUrlElement =
          this.shadowRoot!.querySelector<HTMLElement>('#legal-help-page-url');
      if (legalHelpPageUrlElement) {
        this.setupLinkHandlers(
            legalHelpPageUrlElement, FEEDBACK_LEGAL_HELP_URL,
            false /* useAppWindow */);
      }

      const privacyPolicyUrlElement =
          this.shadowRoot!.querySelector<HTMLElement>('#privacy-policy-url');
      if (privacyPolicyUrlElement) {
        this.setupLinkHandlers(
            privacyPolicyUrlElement, FEEDBACK_PRIVACY_POLICY_URL,
            false /* useAppWindow */);
      }

      const termsOfServiceUrlElement =
          this.shadowRoot!.querySelector<HTMLElement>('#terms-of-service-url');
      if (termsOfServiceUrlElement) {
        this.setupLinkHandlers(
            termsOfServiceUrlElement, FEEDBACK_TERM_OF_SERVICE_URL,
            false /* useAppWindow */);
      }
    }

    // Make sure our focus starts on the description field.
    this.getRequiredElement('#description-text').focus();

    return Promise.all([whenScreenshotUpdated, whenEmailUpdated])
        .then(() => {});
  }

  private async sendFeedbackReport(useSystemInfo: boolean) {
    const ID = Math.round(Date.now() / 1000);
    const FLOW = this.feedbackInfo.flow;

    const result = await FeedbackBrowserProxyImpl.getInstance().sendFeedback(
        this.feedbackInfo, useSystemInfo, this.formOpenTime);

    if (result.status === chrome.feedbackPrivate.Status.SUCCESS) {
      if (FLOW !== chrome.feedbackPrivate.FeedbackFlow.LOGIN &&
          result.landingPageType !==
              chrome.feedbackPrivate.LandingPageType.NO_LANDING_PAGE) {
        const landingPage = result.landingPageType ===
                chrome.feedbackPrivate.LandingPageType.NORMAL ?
            FEEDBACK_LANDING_PAGE :
            FEEDBACK_LANDING_PAGE_TECHSTOP;
        OpenWindowProxyImpl.getInstance().openUrl(landingPage);
      }
    } else {
      console.warn(
          'Feedback: Report for request with ID ' + ID +
          ' will be sent later.');
    }
    this.scheduleWindowClose();
  }

  /**
   * Reads the selected file when the user selects a file.
   * @param fileSelectedEvent The onChanged event for the file input box.
   */
  private onFileSelected(fileSelectedEvent: Event) {
    // <if expr="chromeos_ash">
    // This is needed on CrOS. Otherwise, the feedback window will stay behind
    // the Chrome window.
    FeedbackBrowserProxyImpl.getInstance().showDialog();
    // </if>

    const file = (fileSelectedEvent.target as HTMLInputElement).files![0];
    if (!file) {
      // User canceled file selection.
      this.attachedFileBlob = null;
      return;
    }

    if (file.size > MAX_ATTACH_FILE_SIZE) {
      this.getRequiredElement('#attach-error').hidden = false;

      // Clear our selected file.
      this.getRequiredElement<HTMLInputElement>('#attach-file').value = '';
      this.attachedFileBlob = null;
      return;
    }

    this.attachedFileBlob = file.slice();
  }

  /**
   * Called when user opens the file dialog. Hide 'attach-error' before file
   * dialog is open to prevent a11y bug https://crbug.com/1020047
   */
  private onOpenFileDialog() {
    this.getRequiredElement('#attach-error').hidden = true;
  }

  /**
   * Clears the file that was attached to the report with the initial request.
   * Instead we will now show the attach file button in case the user wants to
   * attach another file.
   */
  private clearAttachedFile() {
    this.getRequiredElement('#custom-file-container').hidden = true;
    this.attachedFileBlob = null;
    this.feedbackInfo.attachedFile = undefined;
    this.getRequiredElement('#attach-file').hidden = false;
  }

  /**
   * Sets up the event handlers for the given |anchorElement|.
   * @param anchorElement The <a> html element.
   * @param url The destination URL for the link.
   * @param useAppWindow true if the URL should be opened inside a new App
   *     Window, false if it should be opened in a new tab.
   */
  private setupLinkHandlers(
      anchorElement: HTMLElement, url: string, useAppWindow: boolean) {
    anchorElement.onclick = (e: Event) => {
      e.preventDefault();
      if (useAppWindow) {
        openUrlInAppWindow(url);
      } else {
        window.open(url, '_blank');
      }
    };

    anchorElement.onauxclick = (e: Event) => {
      e.preventDefault();
    };
  }

  /**
   * Checks if any keywords related to bluetooth have been typed. If they are,
   * we show the bluetooth logs option, otherwise hide it.
   * @param inputEvent The input event for the description textarea.
   */
  private checkForSendBluetoothLogs(inputEvent: Event) {
    const value = (inputEvent.target as HTMLInputElement).value;
    const isRelatedToBluetooth = BT_REGEX.test(value) ||
        CANNOT_CONNECT_REGEX.test(value) || TETHER_REGEX.test(value) ||
        SMART_LOCK_REGEX.test(value) || NEARBY_SHARE_REGEX.test(value) ||
        FAST_PAIR_REGEX.test(value) || BT_DEVICE_REGEX.test(value);
    this.getRequiredElement('#bluetooth-checkbox-container').hidden =
        !isRelatedToBluetooth;
  }

  /**
   * Checks if any keywords have associated questionnaire in a domain. If so,
   * we append the questionnaire in
   * getRequiredElement('description-text').
   * @param inputEvent The input event for the description textarea.
   */
  private checkForShowQuestionnaire(inputEvent: Event) {
    const toAppend = [];

    // Match user-entered description before the questionnaire to reduce false
    // positives due to matching the questionnaire questions and answers.
    const value = (inputEvent.target as HTMLInputElement).value;
    const questionnaireBeginPos = value.indexOf(questionnaireBegin);
    const matchedText = questionnaireBeginPos >= 0 ?
        value.substring(0, questionnaireBeginPos) :
        value;

    if (BT_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['bluetooth']);
    }

    if (WIFI_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['wifi']);
    }

    if (CELLULAR_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['cellular']);
    }

    if (DISPLAY_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['display']);
    }

    if (THUNDERBOLT_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['thunderbolt']);
    } else if (USB_REGEX.test(matchedText)) {
      toAppend.push(...domainQuestions['usb']);
    }

    if (toAppend.length === 0) {
      return;
    }

    const textarea =
        this.getRequiredElement<HTMLTextAreaElement>('#description-text');
    const savedCursor = textarea.selectionStart;
    if (Object.keys(this.appendedQuestions).length === 0) {
      textarea.value += '\n\n' + questionnaireBegin + '\n';
      this.getRequiredElement('#questionnaire-notification').textContent =
          questionnaireNotification;
    }

    for (const question of toAppend) {
      if (question in this.appendedQuestions) {
        continue;
      }

      textarea.value += '* ' + question + ' \n';
      this.appendedQuestions[question] = true;
    }

    // After appending text, the web engine automatically moves the cursor to
    // the end of the appended text, so we need to move the cursor back to where
    // the user was typing before.
    textarea.selectionEnd = savedCursor;
  }

  /**
   * Updates the description-text box based on whether it was valid.
   * If invalid, indicate an error to the user. If valid, remove indication of
   * the error.
   */
  private updateDescription(wasValid: boolean) {
    // Set visibility of the alert text for users who don't use a screen
    // reader.
    this.getRequiredElement('#description-empty-error').hidden = wasValid;

    // Change the textarea's aria-labelled by to ensure the screen reader does
    // (or doesn't) read the error, as appropriate.
    // If it does read the error, it should do so _before_ it reads the normal
    // description.
    const description =
        this.getRequiredElement<HTMLTextAreaElement>('#description-text');
    description.setAttribute(
        'aria-labelledby',
        (wasValid ? '' : 'description-empty-error ') + 'free-form-text');
    // Indicate whether input is valid.
    description.setAttribute('aria-invalid', String(!wasValid));
    if (!wasValid) {
      // Return focus to field so user can correct error.
      description.focus();
    }

    // We may have added or removed a line of text, so make sure the app window
    // is the right size.
    this.resizeAppWindow();
  }

  /**
   * Sends the report; after the report is sent, we need to be redirected to
   * the landing page, but we shouldn't be able to navigate back, hence
   * we open the landing page in a new tab and sendReport closes this tab.
   * @return Whether the report was sent.
   */
  private sendReport(): boolean {
    const textarea =
        this.getRequiredElement<HTMLTextAreaElement>('#description-text');
    if (textarea.value.length === 0) {
      this.updateDescription(false);
      return false;
    }
    // This isn't strictly necessary, since if we get past this point we'll
    // succeed, but for future-compatibility (and in case we later add more
    // failure cases after this), re-hide the alert and reset the aria label.
    this.updateDescription(true);

    // Prevent double clicking from sending additional reports.
    this.getRequiredElement<HTMLButtonElement>('#send-report-button').disabled =
        true;
    if (!this.feedbackInfo.attachedFile && this.attachedFileBlob) {
      this.feedbackInfo.attachedFile = {
        name: this.getRequiredElement<HTMLInputElement>('#attach-file').value,
        data: this.attachedFileBlob,
      };
    }

    const consentCheckboxValue: boolean =
        this.getRequiredElement<HTMLInputElement>('#consent-checkbox').checked;
    this.feedbackInfo.systemInformation = [
      {
        key: 'feedbackUserCtlConsent',
        value: String(consentCheckboxValue),
      },
    ];

    const isAiFlow: boolean =
        this.feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.AI;
    const isSeaPenFlow: boolean|undefined =
        isAiFlow && this.feedbackInfo.aiMetadata?.includes('from_sea_pen');
    if (isAiFlow) {
      this.feedbackInfo.isOffensiveOrUnsafe =
          this.getRequiredElement<HTMLInputElement>('#offensive-checkbox')
              .checked;
      if (isSeaPenFlow ||
          !this.getRequiredElement<HTMLInputElement>('#log-id-checkbox')
               .checked) {
        this.feedbackInfo.aiMetadata = undefined;
      }
    }

    this.feedbackInfo.description = textarea.value;
    this.feedbackInfo.pageUrl =
        this.getRequiredElement<HTMLInputElement>('#page-url-text').value;
    this.feedbackInfo.email =
        this.getRequiredElement<HTMLSelectElement>('#user-email-drop-down')
            .value;

    let useSystemInfo = false;
    let useHistograms = false;
    const checkbox =
        this.shadowRoot!.querySelector<HTMLInputElement>('#sys-info-checkbox');
    // SeaPen flow doesn't collect system info data.
    if (checkbox != null && checkbox.checked && !isSeaPenFlow) {
      // Send histograms along with system info.
      useHistograms = true;
      useSystemInfo = true;
    }

    const autofillCheckbox = this.shadowRoot!.querySelector<HTMLInputElement>(
        '#autofill-metadata-checkbox');
    if (autofillCheckbox != null && autofillCheckbox.checked &&
        !this.getRequiredElement('#autofill-checkbox-container').hidden) {
      this.feedbackInfo.sendAutofillMetadata = true;
    }

    this.feedbackInfo.sendHistograms = useHistograms;

    if (this.getRequiredElement<HTMLInputElement>('#screenshot-checkbox')
            .checked) {
      // The user is okay with sending the screenshot and tab titles.
      this.feedbackInfo.sendTabTitles = true;
    } else {
      // The user doesn't want to send the screenshot, so clear it.
      this.feedbackInfo.screenshot = undefined;
    }

    let productId: number|undefined =
        parseInt('' + this.feedbackInfo.productId, 10);
    if (isNaN(productId)) {
      // For apps that still use a string value as the |productId|, we must
      // clear that value since the API uses an integer value, and a conflict in
      // data types will cause the report to fail to be sent.
      productId = undefined;
    }
    this.feedbackInfo.productId = productId;

    // Request sending the report, show the landing page (if allowed)
    this.sendFeedbackReport(useSystemInfo);

    return true;
  }

  /**
   * Click listener for the cancel button.
   */
  private cancel(e: Event) {
    e.preventDefault();
    this.scheduleWindowClose();
  }

  private resizeAppWindow() {
    // TODO(crbug.com/1167223): The UI is now controlled by a WebDialog delegate
    // which is set to not resizable for now. If needed, a message handler can
    // be added to respond to resize request.
  }

  /**
   * Close the window after 100ms delay.
   */
  private scheduleWindowClose() {
    setTimeout(() => FeedbackBrowserProxyImpl.getInstance().closeDialog(), 100);
  }

  /**
   * TODO(crbug.com/41481648): A helper function in favor of converting feedback
   * UI from non-web component HTML to PolymerElement. It's better to be
   * replaced by polymer's $ helper dictionary.
   */
  getRequiredElement<T extends HTMLElement = HTMLElement>(query: string): T {
    const el = this.shadowRoot!.querySelector<T>(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feedback-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
