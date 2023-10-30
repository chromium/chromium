// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {htmlEscape} from 'chrome://resources/js/util_ts.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {DangerType, States} from './constants.js';
import {MojomData} from './data.js';
import {PageHandlerInterface} from './downloads.mojom-webui.js';
import {IconLoaderImpl} from './icon_loader.js';
import {getTemplate} from './item.html.js';

export interface DownloadsItemElement {
  $: {
    'controlled-by': HTMLElement,
    'file-icon': HTMLImageElement,
    'file-link': HTMLAnchorElement,
    'remove': HTMLElement,
    'url': HTMLAnchorElement,
  };
}

const DownloadsItemElementBase = FocusRowMixin(PolymerElement);

export class DownloadsItemElement extends DownloadsItemElementBase {
  static get is() {
    return 'downloads-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Object,

      completelyOnDisk_: {
        computed: 'computeCompletelyOnDisk_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      hasShowInFolderLink_: {
        computed: 'computeHasShowInFolderLink_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      controlledBy_: {
        computed: 'computeControlledBy_(data.byExtId, data.byExtName)',
        type: String,
        value: '',
      },

      controlRemoveFromListAriaLabel_: {
        type: String,
        computed: 'computeControlRemoveFromListAriaLabel_(data.fileName)',
      },

      isActive_: {
        computed: 'computeIsActive_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      isDangerous_: {
        computed: 'computeIsDangerous_(data.state)',
        type: Boolean,
        value: false,
      },

      isMalware_: {
        computed: 'computeIsMalware_(isDangerous_, data.dangerType)',
        type: Boolean,
        value: false,
      },

      isReviewable_: {
        computed: 'computeIsReviewable_(data.isReviewable)',
        type: Boolean,
        value: false,
      },

      isInProgress_: {
        computed: 'computeIsInProgress_(data.state)',
        type: Boolean,
        value: false,
      },

      pauseOrResumeText_: {
        computed: 'computePauseOrResumeText_(isInProgress_, data.resume)',
        type: String,
        observer: 'updatePauseOrResumeClass_',
      },

      showCancel_: {
        computed: 'computeShowCancel_(data.state)',
        type: Boolean,
        value: false,
      },

      showProgress_: {
        computed: 'computeShowProgress_(showCancel_, data.percent)',
        type: Boolean,
        value: false,
      },

      showOpenNow_: {
        computed: 'computeShowOpenNow_(data.state)',
        type: Boolean,
        value: false,
      },

      showDeepScan_: {
        computed: 'computeShowDeepScan_(data.state)',
        type: Boolean,
        value: false,
      },

      useFileIcon_: Boolean,
    };
  }

  static get observers() {
    return [
      // TODO(dbeam): this gets called way more when I observe data.byExtId
      // and data.byExtName directly. Why?
      'observeControlledBy_(controlledBy_)',
      'observeIsDangerous_(isDangerous_, data)',
      'restoreFocusAfterCancelIfNeeded_(data)',
    ];
  }

  data: MojomData;
  private mojoHandler_: PageHandlerInterface|null = null;
  private controlledBy_: string;
  private isActive_: boolean;
  private isDangerous_: boolean;
  private isInProgress_: boolean;
  private pauseOrResumeText_: string;
  private showCancel_: boolean;
  private showProgress_: boolean;
  private useFileIcon_: boolean;
  private restoreFocusAfterCancel_: boolean = false;
  override overrideCustomEquivalent: boolean;

  constructor() {
    super();

    /** Used by FocusRowMixin. */
    this.overrideCustomEquivalent = true;
  }

  /** @override */
  override ready() {
    super.ready();

    this.setAttribute('role', 'row');
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  }

  focusOnRemoveButton() {
    focusWithoutInk(this.$.remove);
  }

  /** Overrides FocusRowMixin. */
  override getCustomEquivalent(sampleElement: HTMLElement): HTMLElement|null {
    if (sampleElement.getAttribute('focus-type') === 'cancel') {
      return this.shadowRoot!.querySelector('[focus-type="retry"]');
    }
    if (sampleElement.getAttribute('focus-type') === 'retry') {
      return this.shadowRoot!.querySelector('[focus-type="pauseOrResume"]');
    }
    return null;
  }

  getFileIcon(): HTMLImageElement {
    return this.$['file-icon'];
  }

  /**
   * @return A JS string of the display URL.
   */
  private getDisplayUrlStr_(displayUrl: String16): string {
    return mojoString16ToString(displayUrl);
  }

  private computeClass_(): string {
    const classes = [];

    if (this.isActive_) {
      classes.push('is-active');
    }

    if (this.isDangerous_) {
      classes.push('dangerous');
    }

    if (this.showProgress_) {
      classes.push('show-progress');
    }

    return classes.join(' ');
  }

  private computeCompletelyOnDisk_(): boolean {
    return this.data.state === States.COMPLETE &&
        !this.data.fileExternallyRemoved;
  }

  private computeHasShowInFolderLink_(): boolean {
    return loadTimeData.getBoolean('hasShowInFolder') &&
        this.computeCompletelyOnDisk_();
  }

  private computeControlledBy_(): string {
    if (!this.data.byExtId || !this.data.byExtName) {
      return '';
    }

    const url = `chrome://extensions/?id=${this.data.byExtId}`;
    const name = this.data.byExtName;
    return loadTimeData.getStringF('controlledByUrl', url, htmlEscape(name));
  }

  private computeControlRemoveFromListAriaLabel_(): string {
    return loadTimeData.getStringF(
        'controlRemoveFromListAriaLabel', this.data.fileName);
  }

  private computeDate_(): string {
    assert(typeof this.data.hideDate === 'boolean');
    if (this.data.hideDate) {
      return '';
    }
    return this.data.sinceString || this.data.dateString;
  }

  private computeDescriptionVisible_(): boolean {
    return this.computeDescription_() !== '';
  }

  private computeDescription_(): string {
    const data = this.data;

    switch (data.state) {
      case States.COMPLETE:
        switch (data.dangerType) {
          case DangerType.DEEP_SCANNED_SAFE:
            return loadTimeData.getString('deepScannedSafeDesc');
          case DangerType.DEEP_SCANNED_OPENED_DANGEROUS:
            return loadTimeData.getString('deepScannedOpenedDangerousDesc');
        }
        break;

      case States.INSECURE:
        return loadTimeData.getString('insecureDownloadDesc');

      case States.DANGEROUS:
        switch (data.dangerType) {
          case DangerType.DANGEROUS_FILE:
            return loadTimeData.getString('dangerFileDesc');

          case DangerType.DANGEROUS_URL:
          case DangerType.DANGEROUS_CONTENT:
          case DangerType.DANGEROUS_HOST:
            return loadTimeData.getString('dangerDownloadDesc');

          case DangerType.UNCOMMON_CONTENT:
            return loadTimeData.getString('dangerUncommonDesc');

          case DangerType.POTENTIALLY_UNWANTED:
            return loadTimeData.getString('dangerSettingsDesc');

          case DangerType.SENSITIVE_CONTENT_WARNING:
            return loadTimeData.getString('sensitiveContentWarningDesc');

          case DangerType.DANGEROUS_ACCOUNT_COMPROMISE:
            return loadTimeData.getString('accountCompromiseDownloadDesc');
        }
        break;

      case States.ASYNC_SCANNING:
        return loadTimeData.getString('asyncScanningDownloadDesc');
      case States.PROMPT_FOR_SCANNING:
        return loadTimeData.getString('promptForScanningDesc');
      case States.IN_PROGRESS:
      case States.PAUSED:  // Fallthrough.
        return data.progressStatusText;

      case States.INTERRUPTED:
        switch (data.dangerType) {
          case DangerType.SENSITIVE_CONTENT_BLOCK:
            return loadTimeData.getString('sensitiveContentBlockedDesc');
          case DangerType.BLOCKED_TOO_LARGE:
            return loadTimeData.getString('blockedTooLargeDesc');
          case DangerType.BLOCKED_PASSWORD_PROTECTED:
            return loadTimeData.getString('blockedPasswordProtectedDesc');
        }
    }

    return '';
  }

  private computeIcon_(): string {
    if (this.data) {
      const dangerType = this.data.dangerType as DangerType;
      if ((loadTimeData.getBoolean('requestsApVerdicts') &&
           dangerType === DangerType.UNCOMMON_CONTENT) ||
          dangerType === DangerType.SENSITIVE_CONTENT_WARNING) {
        return 'cr:warning';
      }

      const ERROR_TYPES = [
        DangerType.SENSITIVE_CONTENT_BLOCK,
        DangerType.BLOCKED_TOO_LARGE,
        DangerType.BLOCKED_PASSWORD_PROTECTED,
      ];
      if (ERROR_TYPES.includes(dangerType)) {
        return 'cr:error';
      }

      if (this.data.state === States.ASYNC_SCANNING) {
        return 'cr:info';
      }

      if (this.data.state === States.PROMPT_FOR_SCANNING) {
        return 'cr:warning';
      }
    }
    if (this.isDangerous_) {
      return 'cr:error';
    }
    if (!this.useFileIcon_) {
      return 'cr:insert-drive-file';
    }
    return '';
  }

  private computeIconColor_(): string {
    if (this.data) {
      const dangerType = this.data.dangerType as DangerType;
      if ((loadTimeData.getBoolean('requestsApVerdicts') &&
           dangerType === DangerType.UNCOMMON_CONTENT) ||
          dangerType === DangerType.SENSITIVE_CONTENT_WARNING) {
        return 'yellow';
      }

      const WARNING_TYPES = [
        DangerType.SENSITIVE_CONTENT_BLOCK,
        DangerType.BLOCKED_TOO_LARGE,
        DangerType.BLOCKED_PASSWORD_PROTECTED,
      ];
      if (WARNING_TYPES.includes(dangerType)) {
        return 'red';
      }

      if (this.data.state === States.ASYNC_SCANNING) {
        return 'grey';
      }

      if (this.data.state === States.PROMPT_FOR_SCANNING) {
        return 'yellow';
      }
    }
    if (this.isDangerous_) {
      return 'red';
    }
    if (!this.useFileIcon_) {
      return 'paper-grey';
    }
    return '';
  }

  private computeIsActive_(): boolean {
    return this.data.state !== States.CANCELLED &&
        this.data.state !== States.INTERRUPTED &&
        !this.data.fileExternallyRemoved;
  }

  private computeIsDangerous_(): boolean {
    return this.data.state === States.DANGEROUS ||
        this.data.state === States.INSECURE;
  }

  private computeIsInProgress_(): boolean {
    return this.data.state === States.IN_PROGRESS;
  }

  private computeIsMalware_(): boolean {
    return this.isDangerous_ &&
        (this.data.dangerType === DangerType.DANGEROUS_CONTENT ||
         this.data.dangerType === DangerType.DANGEROUS_HOST ||
         this.data.dangerType === DangerType.DANGEROUS_URL ||
         this.data.dangerType === DangerType.POTENTIALLY_UNWANTED ||
         this.data.dangerType === DangerType.DANGEROUS_ACCOUNT_COMPROMISE);
  }

  private computeIsReviewable_(): boolean {
    return this.data.isReviewable;
  }

  private toggleButtonClass_() {
    this.shadowRoot!.querySelector('#pauseOrResume')!.classList.toggle(
        'action-button',
        this.pauseOrResumeText_ === loadTimeData.getString('controlResume'));
  }

  private updatePauseOrResumeClass_() {
    if (!this.pauseOrResumeText_) {
      return;
    }

    // Wait for dom-if to switch to true, in case the text has just changed
    // from empty.
    beforeNextRender(this, () => this.toggleButtonClass_());
  }

  private computePauseOrResumeText_(): string {
    if (this.data === undefined) {
      return '';
    }

    if (this.isInProgress_) {
      return loadTimeData.getString('controlPause');
    }
    if (this.data.resume) {
      return loadTimeData.getString('controlResume');
    }
    return '';
  }

  private computeRemoveStyle_(): string {
    const canDelete = loadTimeData.getBoolean('allowDeletingHistory');
    const hideRemove = this.isDangerous_ || this.showCancel_ || !canDelete;
    return hideRemove ? 'visibility: hidden' : '';
  }

  private computeShowCancel_(): boolean {
    return this.data.state === States.IN_PROGRESS ||
        this.data.state === States.PAUSED ||
        this.data.state === States.ASYNC_SCANNING;
  }

  private computeShowProgress_(): boolean {
    return this.showCancel_ && this.data.percent >= -1 &&
        this.data.state !== States.ASYNC_SCANNING &&
        this.data.state !== States.PROMPT_FOR_SCANNING;
  }

  private computeShowOpenNow_(): boolean {
    const allowOpenNow = loadTimeData.getBoolean('allowOpenNow');
    return this.data.state === States.ASYNC_SCANNING && allowOpenNow;
  }

  private computeShowDeepScan_(): boolean {
    return this.data.state === States.PROMPT_FOR_SCANNING;
  }

  private computeTag_(): string {
    switch (this.data.state) {
      case States.CANCELLED:
        return loadTimeData.getString('statusCancelled');

      case States.INTERRUPTED:
        return this.data.lastReasonText;

      case States.COMPLETE:
        return this.data.fileExternallyRemoved ?
            loadTimeData.getString('statusRemoved') :
            '';
    }

    return '';
  }

  private isIndeterminate_(): boolean {
    return this.data.percent === -1;
  }

  private observeControlledBy_() {
    this.$['controlled-by'].innerHTML = sanitizeInnerHtml(this.controlledBy_);
    if (this.controlledBy_) {
      const link = this.shadowRoot!.querySelector('#controlled-by a');
      link!.setAttribute('focus-row-control', '');
      link!.setAttribute('focus-type', 'controlledBy');
    }
  }

  private observeIsDangerous_() {
    const removeFileUrlLinks = () => {
      this.$.url.removeAttribute('href');
      this.$['file-link'].removeAttribute('href');
    };

    if (!this.data) {
      return;
    }

    const OVERRIDDEN_ICON_TYPES = [
      DangerType.SENSITIVE_CONTENT_BLOCK,
      DangerType.BLOCKED_TOO_LARGE,
      DangerType.BLOCKED_PASSWORD_PROTECTED,
    ];

    // Handle various dangerous cases.
    if (this.isDangerous_) {
      removeFileUrlLinks();
      this.useFileIcon_ = false;
      return;
    }
    if (OVERRIDDEN_ICON_TYPES.includes(this.data.dangerType as DangerType)) {
      this.useFileIcon_ = false;
      return;
    }
    if (this.data.state === States.ASYNC_SCANNING) {
      this.useFileIcon_ = false;
      return;
    }
    if (this.data.state === States.PROMPT_FOR_SCANNING) {
      this.useFileIcon_ = false;
      return;
    }

    // The file is not dangerous. Link the url if supplied.
    if (this.data.url) {
      this.$.url.href = this.data.url.url;
    } else {
      removeFileUrlLinks();
    }

    const path = this.data.filePath;
    IconLoaderImpl.getInstance()
        .loadIcon(this.$['file-icon'], path)
        .then(success => {
          if (path === this.data.filePath &&
              this.data.state !== States.ASYNC_SCANNING) {
            this.useFileIcon_ = success;
          }
        });
  }

  private onCancelClick_() {
    this.restoreFocusAfterCancel_ = true;
    this.mojoHandler_!.cancel(this.data.id);
  }

  private onDiscardDangerousClick_() {
    this.mojoHandler_!.discardDangerous(this.data.id);
  }

  private onOpenNowClick_() {
    this.mojoHandler_!.openDuringScanningRequiringGesture(this.data.id);
  }

  private onDeepScanClick_() {
    this.mojoHandler_!.deepScan(this.data.id);
  }

  private onBypassDeepScanClick_() {
    this.mojoHandler_!.bypassDeepScanRequiringGesture(this.data.id);
  }

  private onReviewDangerousClick_() {
    this.mojoHandler_!.reviewDangerousRequiringGesture(this.data.id);
  }

  private onDragStart_(e: Event) {
    e.preventDefault();
    this.mojoHandler_!.drag(this.data.id);
  }

  private onFileLinkClick_(e: Event) {
    e.preventDefault();
    this.mojoHandler_!.openFileRequiringGesture(this.data.id);
  }

  private onUrlClick_() {
    if (!this.data.url) {
      return;
    }
    chrome.send(
        'metricsHandler:recordAction', ['Downloads_OpenUrlOfDownloadedItem']);
  }

  private onPauseOrResumeClick_() {
    if (this.isInProgress_) {
      this.mojoHandler_!.pause(this.data.id);
    } else {
      this.mojoHandler_!.resume(this.data.id);
    }
  }

  private onRemoveClick_(e: Event) {
    this.mojoHandler_!.remove(this.data.id);
    const pieces = loadTimeData.getSubstitutedStringPieces(
                       loadTimeData.getString('toastRemovedFromList'),
                       this.data.fileName) as unknown as
        Array<{collapsible: boolean, value: string, arg?: string}>;

    pieces.forEach(p => {
      // Make the file name collapsible.
      p.collapsible = !!p.arg;
    });
    const canUndo = !this.data.isDangerous && !this.data.isInsecure;
    getToastManager().showForStringPieces(pieces, /* hideSlotted= */ !canUndo);

    // Stop propagating a click to the document to remove toast.
    e.stopPropagation();
    e.preventDefault();
  }

  private onRetryClick_() {
    this.mojoHandler_!.retryDownload(this.data.id);
  }

  private onSaveDangerousClick_() {
    this.mojoHandler_!.saveDangerousRequiringGesture(this.data.id);
  }

  private onShowClick_() {
    this.mojoHandler_!.show(this.data.id);
  }

  private restoreFocusAfterCancelIfNeeded_() {
    if (!this.restoreFocusAfterCancel_) {
      return;
    }
    this.restoreFocusAfterCancel_ = false;
    setTimeout(() => {
      const element = this.getFocusRow().getFirstFocusable('retry');
      if (element) {
        (element as HTMLElement).focus();
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-item': DownloadsItemElement;
  }
}

customElements.define(DownloadsItemElement.is, DownloadsItemElement);
