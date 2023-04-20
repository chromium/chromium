// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './file_path.mojom-lite.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState, ScanCompleteAction} from './scanning_app_types.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'scan-done-section' shows the post-scan user options.
 */
Polymer({
  is: 'scan-done-section',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /** @type {number} */
    numFilesSaved: {
      type: Number,
      observer: 'onNumFilesSavedChange_',
    },

    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    scannedFilePaths: Array,

    /** @type {string} */
    selectedFileType: String,

    /** @type {string} */
    selectedFolder: String,

    /** @private {string} */
    fileSavedTextContent_: String,

    /** @private {boolean} */
    showEditButton_: {
      type: Boolean,
      computed: 'computeShowEditButton_(selectedFileType)',
    },

    /** @private {string} */
    editButtonLabel_: String,
  },

  observers: ['setFileSavedTextContent_(numFilesSaved, selectedFolder)'],

  /** @override */
  created() {
    // ScanningBrowserProxy is initialized when scanning_app.js is created.
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
  },

  /** @private */
  onDoneClick_() {
    this.browserProxy_.recordScanCompleteAction(
        ScanCompleteAction.DONE_BUTTON_CLICKED);
    this.fire('done-click');
  },

  /** @private */
  showFileInLocation_() {
    assert(this.scannedFilePaths.length !== 0);

    this.browserProxy_.recordScanCompleteAction(
        ScanCompleteAction.FILES_APP_OPENED);
    this.browserProxy_
        .showFileInLocation(this.scannedFilePaths.slice(-1)[0].path)
        .then(
            /* @type {boolean} */ (succesful) => {
              if (!succesful) {
                this.fire('file-not-found');
              }
            });
  },

  /** @private */
  setFileSavedTextContent_() {
    this.browserProxy_.getPluralString('fileSavedText', this.numFilesSaved)
        .then(
            /* @type {string} */ (pluralString) => {
              this.fileSavedTextContent_ =
                  this.getAriaLabelledContent_(loadTimeData.substituteString(
                      pluralString, this.selectedFolder));
              const linkElement = this.$$('#folderLink');
              linkElement.setAttribute('href', '#');
              linkElement.addEventListener(
                  'click', () => this.showFileInLocation_());
            });
  },

  /**
   * Takes a localized string that contains exactly one anchor tag and labels
   * the string contained within the anchor tag with the entire localized
   * string. The string should not be bound by element tags. The string should
   * not contain any elements other than the single anchor tagged element that
   * will be aria-labelledby the entire string.
   * @param {string} localizedString
   * @return {string}
   * @private
   */
  getAriaLabelledContent_(localizedString) {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType == Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `id${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }

      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType == Node.ELEMENT_NODE && node.nodeName == 'A') {
        ariaLabelledByIds.push(node.id);
        return;
      }
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    anchorTags[0].setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));

    return tempEl.innerHTML;
  },

  /** @private */
  computeShowEditButton_() {
    return this.selectedFileType !==
        ash.scanning.mojom.FileType.kPdf.toString();
  },

  /** @private */
  openMediaApp_() {
    assert(this.scannedFilePaths.length !== 0);

    this.browserProxy_.recordScanCompleteAction(
        ScanCompleteAction.MEDIA_APP_OPENED);
    this.browserProxy_.openFilesInMediaApp(
        this.scannedFilePaths.map(filePath => filePath.path));
  },

  /** @private */
  onNumFilesSavedChange_() {
    this.browserProxy_.getPluralString('editButtonLabel', this.numFilesSaved)
        .then(
            /* @type {string} */ (pluralString) => {
              this.editButtonLabel_ = pluralString;
            });
  },
});
