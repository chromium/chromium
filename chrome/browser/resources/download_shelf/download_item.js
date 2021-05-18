// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download item.
 */

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './download_button.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {DangerType, DownloadItem, DownloadMode, DownloadState, MixedContentStatus} from './download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

/** @enum {string} */
const DisplayMode = {
  // Shows icon + filename + context menu button.
  kNormal: 'normal',
  // Shows icon + warning text + discard button + context menu button.
  kWarn: 'warn',
  // Shows icon + warning text + save button + discard button.
  kWarnSave: 'warn-save'
};

export class DownloadItemElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {DownloadItem} */
    this.item_;

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    this.$('#dropdown-button')
        .addEventListener('click', e => this.onDropdownButtonClick_(e));
    this.$('#discard-button')
        .addEventListener('click', e => this.onDiscardButtonClick_(e));
    this.addEventListener('contextmenu', e => this.onContextMenu_(e));

    this.$('#discard-button').innerText =
        loadTimeData.getString('discardButtonText');
  }

  /** @param {DownloadItem} value */
  set item(value) {
    if (this.item_ === value) {
      return;
    }
    this.item_ = value;
    this.update_();
  }

  /** @return {DownloadItem} */
  get item() {
    return this.item_;
  }

  /**
   * @private
   * @return {string}
   */
  get clampedWarningText_() {
    // Views uses ui/gfx/text_elider.cc to elide text given a maximum width.
    // For simplicity, we instead elide text by restricting text length.
    const maxFilenameLength = 19;
    const warningText = this.item.warningText;
    if (!warningText) {
      return '';
    }

    const filepath = this.item.fileNameDisplayString;
    const filename = filepath.substring(filepath.lastIndexOf('/') + 1);
    return warningText.replace(
        filename, this.elideFilename_(filename, maxFilenameLength));
  }

  /** @private */
  update_() {
    const item = this.item_;
    if (!item) {
      return;
    }
    const downloadElement = this.$('.download-item');
    const filePath = item.fileNameDisplayString;
    this.$('#filename').innerText =
        filePath.substring(filePath.lastIndexOf('/') + 1);

    const statusTextElement = this.$('#status-text');
    const statusText = (!item.shouldPromoteOrigin || !item.originalUrl.url) ?
        item.statusText :
        new URL(item.originalUrl.url).origin;
    statusTextElement.innerText = statusText;

    downloadElement.dataset.state = item.state;
    switch (item.state) {
      case DownloadState.kInProgress:
        this.progress = item.totalBytes > 0 ?
            Number(item.receivedBytes) / Number(item.totalBytes) :
            0;
        break;
      case DownloadState.kComplete:
        this.progress = 1;
        break;
      case DownloadState.kInterrupted:
        this.progress = 0;
        break;
    }

    if (item.isPaused) {
      downloadElement.dataset.paused = true;
    } else {
      delete downloadElement.dataset.paused;
    }

    this.apiProxy_.getFileIcon(item.id).then(icon => {
      this.$('#file-icon').src = icon;
    });

    if (item.mode === DownloadMode.kNormal) {
      downloadElement.dataset.displayMode = DisplayMode.kNormal;
    } else if (
        item.mode === DownloadMode.kDangerous ||
        item.mode === DownloadMode.kMixedContentWarn) {
      downloadElement.dataset.displayMode = DisplayMode.kWarnSave;
    } else {
      downloadElement.dataset.displayMode = DisplayMode.kWarn;
    }

    this.$('#save-button').innerText = item.warningConfirmButtonText;
    this.$('#warning-text').innerText = this.clampedWarningText_;
  }

  /** @param {number} value */
  set progress(value) {
    this.$('.progress')
        .style.setProperty('--download-progress', value.toString());
  }

  /** @param {!Event} e */
  onContextMenu_(e) {
    this.apiProxy_.showContextMenu(
        this.item.id, e.clientX, e.clientY, Date.now());
  }

  /** @param {!Event} e */
  onDropdownButtonClick_(e) {
    // TODO(crbug.com/1182529): Switch to down caret icon when context menu is
    // open.
    const rect = e.target.getBoundingClientRect();
    this.apiProxy_.showContextMenu(
        this.item.id, rect.left, rect.top, Date.now());
  }

  /** @param {!Event} e */
  onDiscardButtonClick_(e) {
    // TODO(crbug.com/1182529): Notify C++ through mojo. Remove this item
    // from download_list.
  }

  /**
   * Elide a filename to a maximum length.
   * The extension of the filename will be kept if it has one.
   * @param {string} s A filename.
   * @param {number} maxlen The maximum length after elided.
   * @private
   */
  elideFilename_(s, maxlen) {
    assert(maxlen > 6);

    if (s.length <= maxlen) {
      return s;
    }

    const extIndex = s.lastIndexOf('.');
    if (extIndex === -1) {
      // |s| does not have an extension.
      return s.substr(0, maxlen - 3) + '...';
    } else {
      const subfix = '...' + s.substr(extIndex);
      return s.substr(0, maxlen - subfix.length) + subfix;
    }
  }
}

customElements.define('download-item', DownloadItemElement);
