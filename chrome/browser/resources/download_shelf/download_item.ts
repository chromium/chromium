// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download item.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import {DownloadItem, DownloadMode, DownloadState} from './download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

enum DisplayMode {
  // Shows icon + filename + context menu button.
  NORMAL = 'normal',
  // Shows icon + warning text + discard button + context menu button.
  WARN = 'warn',
  // Shows icon + warning text + keep button + discard button.
  WARN_KEEP = 'warn-keep',
}

export class DownloadItemElement extends CustomElement {
  static override get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  private item_: DownloadItem;
  private downloadUpdated_: boolean = false;
  private opening_: boolean = false;
  opened: boolean;
  private apiProxy_: DownloadShelfApiProxy;


  constructor() {
    super();

    this.opened = false;
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    this.$('#shadow-mask')!.addEventListener(
        'click', () => this.onOpenButtonClick_());
    this.$('#dropdown-button')!.addEventListener(
        'click', e => this.onDropdownButtonClick_(e));
    const discardButton = this.$('#discard-button') as HTMLElement;
    discardButton.innerText = loadTimeData.getString('discardButtonText');
    discardButton.addEventListener('click', () => this.onDiscardButtonClick_());
    this.$('#keep-button')!.addEventListener(
        'click', () => this.onKeepButtonClick_());
    this.addEventListener('contextmenu', e => this.onContextMenu_(e));

    this.$('.progress-indicator')!.addEventListener('animationend', () => {
      this.$('.progress-indicator')!.classList.remove(
          'download-complete-animation');
    });
  }

  onDownloadUpdated(item: DownloadItem) {
    this.downloadUpdated_ = true;
    this.item_ = item;
    this.update_();
  }

  set item(value: DownloadItem) {
    if (this.item_ === value) {
      return;
    }
    this.item_ = value;
    this.update_();
  }

  get item(): DownloadItem {
    return this.item_;
  }

  private get clampedWarningText_(): string {
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

  set opening(value: boolean) {
    if (this.opening_ !== value) {
      this.opening_ = value;
      this.update_();
    }
  }

  private update_() {
    const item = this.item_;
    if (!item) {
      return;
    }
    const downloadElement = this.$('.download-item') as HTMLElement;
    const filePath = item.fileNameDisplayString;
    let fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
    if (this.opening_) {
      fileName = loadTimeData.getStringF('downloadStatusOpeningText', fileName);
    }
    (this.$('#filename') as HTMLElement).innerText = fileName;

    const statusTextElement = this.$('#status-text') as HTMLElement;
    const statusText = (!item.shouldPromoteOrigin || !item.originalUrl.url) ?
        item.statusText :
        new URL(item.originalUrl.url).origin;
    statusTextElement.innerText = statusText;

    downloadElement.dataset['state'] = item.state.toString();
    if (item.mode === DownloadMode.kNormal) {
      switch (item.state) {
        case DownloadState.kInProgress:
          this.progress = item.totalBytes > 0 ?
              Number(item.receivedBytes) / Number(item.totalBytes) :
              0;
          break;
        case DownloadState.kComplete:
          this.progress = 1;
          // Only start animation if it's called from OnDownloadUpdated.
          if (this.downloadUpdated_) {
            this.$('.progress-indicator')!.classList.add(
                'download-complete-animation');
          }
          break;
        case DownloadState.kInterrupted:
          this.progress = 0;
          break;
      }
    }

    if (item.isPaused) {
      downloadElement.dataset['paused'] = 'true';
    } else {
      delete downloadElement.dataset['paused'];
    }

    this.apiProxy_.getFileIcon(item.id).then(icon => {
      (this.$('#file-icon') as HTMLImageElement).src = icon;
    });

    if (item.mode === DownloadMode.kNormal) {
      downloadElement.dataset['displayMode'] = DisplayMode.NORMAL;
    } else if (
        item.mode === DownloadMode.kDangerous ||
        item.mode === DownloadMode.kMixedContentWarn) {
      downloadElement.dataset['displayMode'] = DisplayMode.WARN_KEEP;
    } else {
      downloadElement.dataset['displayMode'] = DisplayMode.WARN;
    }

    (this.$('#keep-button') as HTMLElement).innerText =
        item.warningConfirmButtonText;
    (this.$('#warning-text') as HTMLElement).innerText =
        this.clampedWarningText_;

    this.downloadUpdated_ = false;
  }

  set progress(value: number) {
    (this.$('.progress') as HTMLElement)
        .style.setProperty('--download-progress', value.toString());
  }

  private onContextMenu_(e: MouseEvent) {
    this.apiProxy_.showContextMenu(
        this.item.id, e.clientX, e.clientY, Date.now());
  }

  private onDropdownButtonClick_(e: Event) {
    // TODO(crbug.com/1182529): Switch to down caret icon when context menu is
    // open.
    const rect = (e.target as Element).getBoundingClientRect();
    this.apiProxy_.showContextMenu(
        this.item.id, rect.left, rect.top, Date.now());
  }

  private onDiscardButtonClick_() {
    this.apiProxy_.discardDownload(this.item.id);
  }

  private onKeepButtonClick_() {
    this.apiProxy_.keepDownload(this.item.id);
  }

  /**
   * Elide a filename to a maximum length.
   * The extension of the filename will be kept if it has one.
   * @param s A filename.
   * @param maxlen The maximum length after elided.
   */
  private elideFilename_(s: string, maxlen: number): string {
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

  private onOpenButtonClick_() {
    if (this.opening_) {
      return;
    }
    if (this.item_.mode === DownloadMode.kNormal) {
      this.apiProxy_.openDownload(this.item.id);
    } else {
      // TODO(crbug.com/1182529): Handle the scanning case.
    }
  }
}

customElements.define('download-item', DownloadItemElement);

declare global {
  interface HTMLElementTagNameMap {
    'download-item': DownloadItemElement;
  }
}
