// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {HTMLEscape} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {DangerType, States} from './constants.js';
import {IconLoader} from './icon_loader.js';

Polymer({
  is: 'downloads-item',

  _template: html`{__html_template__}`,

  behaviors: [
    FocusRowBehavior,
  ],

  /** Used by FocusRowBehavior. */
  overrideCustomEquivalent: true,

  properties: {
    /** @type {!downloads.Data} */
    data: Object,

    /** @private */
    completelyOnDisk_: {
      computed: 'computeCompletelyOnDisk_(' +
          'data.state, data.fileExternallyRemoved)',
      type: Boolean,
      value: true,
    },

    /** @private */
    controlledBy_: {
      computed: 'computeControlledBy_(data.byExtId, data.byExtName)',
      type: String,
      value: '',
    },

    /** @private */
    controlRemoveFromListAriaLabel_: {
      type: String,
      computed: 'computeControlRemoveFromListAriaLabel_(data.fileName)',
    },

    /** @private */
    isActive_: {
      computed: 'computeIsActive_(' +
          'data.state, data.fileExternallyRemoved)',
      type: Boolean,
      value: true,
    },

    /** @private */
    isDangerous_: {
      computed: 'computeIsDangerous_(data.state)',
      type: Boolean,
      value: false,
    },

    /** @private */
    isMalware_: {
      computed: 'computeIsMalware_(isDangerous_, data.dangerType)',
      type: Boolean,
      value: false,
    },

    /** @private */
    isInProgress_: {
      computed: 'computeIsInProgress_(data.state)',
      type: Boolean,
      value: false,
    },

    /** @private */
    pauseOrResumeText_: {
      computed: 'computePauseOrResumeText_(isInProgress_, data.resume)',
      type: String,
      observer: 'updatePauseOrResumeClass_',
    },

    /** @private */
    showCancel_: {
      computed: 'computeShowCancel_(data.state)',
      type: Boolean,
      value: false,
    },

    /** @private */
    showProgress_: {
      computed: 'computeShowProgress_(showCancel_, data.percent)',
      type: Boolean,
      value: false,
    },

    /** @private */
    showOpenNow_: {
      computed: 'computeShowOpenNow_(data.state)',
      type: Boolean,
      value: false,
    },

    useFileIcon_: Boolean,
  },

  hostAttributes: {
    role: 'row',
  },

  observers: [
    // TODO(dbeam): this gets called way more when I observe data.byExtId
    // and data.byExtName directly. Why?
    'observeControlledBy_(controlledBy_)',
    'observeIsDangerous_(isDangerous_, data)',
    'restoreFocusAfterCancelIfNeeded_(data)',
  ],

  /** @private {downloads.mojom.PageHandlerInterface} */
  mojoHandler_: null,

  /** @private {boolean} */
  restoreFocusAfterCancel_: false,

  /** @override */
  attached() {
    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  ready() {
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
    this.content = this.$.content;
  },

  focusOnRemoveButton() {
    focusWithoutInk(this.$.remove);
  },

  /** Overrides FocusRowBehavior. */
  getCustomEquivalent(sampleElement) {
    if (sampleElement.getAttribute('focus-type') === 'cancel') {
      return this.$$('[focus-type="retry"]');
    }
    if (sampleElement.getAttribute('focus-type') === 'retry') {
      return this.$$('[focus-type="pauseOrResume"]');
    }
    return null;
  },

  /** @return {!HTMLElement} */
  getFileIcon() {
    return /** @type {!HTMLElement} */ (this.$['file-icon']);
  },

  /**
   * @param {string} url
   * @return {string} A reasonably long URL.
   * @private
   */
  chopUrl_(url) {
    return url.slice(0, 300);
  },

  /** @private */
  computeClass_() {
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
  },

  /**
   * @return {boolean}
   * @private
   */
  computeCompletelyOnDisk_() {
    return this.data.state === States.COMPLETE &&
        !this.data.fileExternallyRemoved;
  },

  /**
   * @return {string}
   * @private
   */
  computeControlledBy_() {
    if (!this.data.byExtId || !this.data.byExtName) {
      return '';
    }

    const url = `chrome://extensions/?id=${this.data.byExtId}`;
    const name = this.data.byExtName;
    return loadTimeData.getStringF('controlledByUrl', url, HTMLEscape(name));
  },

  /**
   * @return {string}
   * @private
   */
  computeControlRemoveFromListAriaLabel_() {
    return loadTimeData.getStringF(
        'controlRemoveFromListAriaLabel', this.data.fileName);
  },

  /**
   * @return {string}
   * @private
   */
  computeDate_() {
    assert(typeof this.data.hideDate === 'boolean');
    if (this.data.hideDate) {
      return '';
    }
    return assert(this.data.sinceString || this.data.dateString);
  },

  /** @private @return {boolean} */
  computeDescriptionVisible_() {
    return this.computeDescription_() !== '';
  },

  /**
   * @return {string}
   * @private
   */
  computeDescription_() {
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

      case States.MIXED_CONTENT:
        return loadTimeData.getString('mixedContentDownloadDesc');

      case States.DANGEROUS:
        const fileName = data.fileName;
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
        }
        break;

      case States.ASYNC_SCANNING:
        return loadTimeData.getString('asyncScanningDownloadDesc');

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
  },

  /**
   * @return {string}
   * @private
   */
  computeIcon_() {
    if (this.data) {
      const dangerType = this.data.dangerType;
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
    }
    if (this.isDangerous_) {
      return 'cr:error';
    }
    if (!this.useFileIcon_) {
      return 'cr:insert-drive-file';
    }
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  computeIconColor_() {
    if (this.data) {
      const dangerType = this.data.dangerType;
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
    }
    if (this.isDangerous_) {
      return 'red';
    }
    if (!this.useFileIcon_) {
      return 'paper-grey';
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsActive_() {
    return this.data.state !== States.CANCELLED &&
        this.data.state !== States.INTERRUPTED &&
        !this.data.fileExternallyRemoved;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsDangerous_() {
    return this.data.state === States.DANGEROUS ||
        this.data.state === States.MIXED_CONTENT;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsInProgress_() {
    return this.data.state === States.IN_PROGRESS;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsMalware_() {
    return this.isDangerous_ &&
        (this.data.dangerType === DangerType.DANGEROUS_CONTENT ||
         this.data.dangerType === DangerType.DANGEROUS_HOST ||
         this.data.dangerType === DangerType.DANGEROUS_URL ||
         this.data.dangerType === DangerType.POTENTIALLY_UNWANTED);
  },

  /** @private */
  toggleButtonClass_() {
    this.$$('#pauseOrResume')
        .classList.toggle(
            'action-button',
            this.pauseOrResumeText_ ===
                loadTimeData.getString('controlResume'));
  },

  /** @private */
  updatePauseOrResumeClass_() {
    if (!this.pauseOrResumeText_) {
      return;
    }

    // Wait for dom-if to switch to true, in case the text has just changed
    // from empty.
    beforeNextRender(this, () => this.toggleButtonClass_());
  },

  /**
   * @return {string}
   * @private
   */
  computePauseOrResumeText_() {
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
  },

  /**
   * @return {string}
   * @private
   */
  computeRemoveStyle_() {
    const canDelete = loadTimeData.getBoolean('allowDeletingHistory');
    const hideRemove = this.isDangerous_ || this.showCancel_ || !canDelete;
    return hideRemove ? 'visibility: hidden' : '';
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowCancel_() {
    return this.data.state === States.IN_PROGRESS ||
        this.data.state === States.PAUSED ||
        this.data.state === States.ASYNC_SCANNING;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowProgress_() {
    return this.showCancel_ && this.data.percent >= -1 &&
        this.data.state !== States.ASYNC_SCANNING;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowOpenNow_() {
    const allowOpenNow = loadTimeData.getBoolean('allowOpenNow');
    return this.data.state === States.ASYNC_SCANNING && allowOpenNow;
  },

  /**
   * @return {string}
   * @private
   */
  computeTag_() {
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
  },

  /**
   * @return {boolean}
   * @private
   */
  isIndeterminate_() {
    return this.data.percent === -1;
  },

  /** @private */
  observeControlledBy_() {
    this.$['controlled-by'].innerHTML = this.controlledBy_;
    if (this.controlledBy_) {
      const link = this.$$('#controlled-by a');
      link.setAttribute('focus-row-control', '');
      link.setAttribute('focus-type', 'controlledBy');
    }
  },

  /** @private */
  observeIsDangerous_() {
    if (!this.data) {
      return;
    }

    const OVERRIDDEN_ICON_TYPES = [
      DangerType.SENSITIVE_CONTENT_BLOCK,
      DangerType.BLOCKED_TOO_LARGE,
      DangerType.BLOCKED_PASSWORD_PROTECTED,
    ];

    if (this.isDangerous_) {
      this.$.url.removeAttribute('href');
      this.useFileIcon_ = false;
    } else if (OVERRIDDEN_ICON_TYPES.includes(this.data.dangerType)) {
      this.useFileIcon_ = false;
    } else if (this.data.state === States.ASYNC_SCANNING) {
      this.useFileIcon_ = false;
    } else {
      this.$.url.href = assert(this.data.url);
      const path = this.data.filePath;
      IconLoader.getInstance()
          .loadIcon(this.$['file-icon'], path)
          .then(success => {
            if (path === this.data.filePath &&
                this.data.state !== States.ASYNC_SCANNING) {
              this.useFileIcon_ = success;
            }
          });
    }
  },

  /** @private */
  onCancelTap_() {
    this.restoreFocusAfterCancel_ = true;
    this.mojoHandler_.cancel(this.data.id);
  },

  /** @private */
  onDiscardDangerousTap_() {
    this.mojoHandler_.discardDangerous(this.data.id);
  },

  /** @private */
  onOpenNowTap_() {
    this.mojoHandler_.openDuringScanningRequiringGesture(this.data.id);
  },

  /**
   * @private
   * @param {Event} e
   */
  onDragStart_(e) {
    e.preventDefault();
    this.mojoHandler_.drag(this.data.id);
  },

  /**
   * @param {Event} e
   * @private
   */
  onFileLinkTap_(e) {
    e.preventDefault();
    this.mojoHandler_.openFileRequiringGesture(this.data.id);
  },

  /** @private */
  onUrlTap_() {
    chrome.send(
        'metricsHandler:recordAction', ['Downloads_OpenUrlOfDownloadedItem']);
  },

  /** @private */
  onPauseOrResumeTap_() {
    if (this.isInProgress_) {
      this.mojoHandler_.pause(this.data.id);
    } else {
      this.mojoHandler_.resume(this.data.id);
    }
  },

  /** @private */
  onRemoveTap_() {
    this.mojoHandler_.remove(this.data.id);
    const pieces = loadTimeData.getSubstitutedStringPieces(
        loadTimeData.getString('toastRemovedFromList'), this.data.fileName);
    pieces.forEach(p => {
      // Make the file name collapsible.
      p.collapsible = !!p.arg;
    });
    const canUndo = !this.data.isDangerous && !this.data.isMixedContent;
    getToastManager().showForStringPieces(
        /**
         * @type {!Array<{collapsible: boolean,
         *                 value: string,
         *                 arg: (string|null)}>}
         */
        (pieces), /* hideSlotted= */ !canUndo);
    if (canUndo) {
      this.fire('iron-announce', {
        text: loadTimeData.getString('undoDescription'),
      });
    }
  },

  /** @private */
  onRetryTap_() {
    this.mojoHandler_.retryDownload(this.data.id);
  },

  /** @private */
  onSaveDangerousTap_() {
    this.mojoHandler_.saveDangerousRequiringGesture(this.data.id);
  },

  /** @private */
  onShowTap_() {
    this.mojoHandler_.show(this.data.id);
  },

  /** @private */
  restoreFocusAfterCancelIfNeeded_() {
    if (!this.restoreFocusAfterCancel_) {
      return;
    }
    this.restoreFocusAfterCancel_ = false;
    setTimeout(() => {
      const element = this.getFocusRow().getFirstFocusable('retry');
      if (element) {
        element.focus();
      }
    });
  },
});
