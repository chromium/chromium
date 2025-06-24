// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import type {ComposeboxPageHandlerRemote} from '../composebox.mojom-webui.js';
import {recordLoadDuration} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export interface ComposeboxElement {
  $: {
    fileInput: HTMLInputElement,
    fileUploadButton: HTMLElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: HTMLElement,
    input: HTMLInputElement,
    composebox: HTMLElement,
  };
}

export class ComposeboxElement extends CrLitElement {
  static get is() {
    return 'ntp-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      attachmentFileTypes_: {type: String},
      files_: {type: Array},
      imageFileTypes_: {type: String},
      submitEnabled_: {
        reflect: true,
        type: Boolean,
      },
      submitting_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  protected accessor attachmentFileTypes_: string =
      loadTimeData.getString('composeboxAttachmentFileTypes');
  protected accessor files_: ComposeboxFile[] = [];
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  protected accessor submitEnabled_: boolean = false;
  protected accessor submitting_: boolean = false;
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private pageHandler_: ComposeboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.pageHandler_.notifySessionStarted();
    recordLoadDuration(
        'NewTabPage.Composebox.FromNTPLoadToSessionStart',
        WindowProxy.getInstance().now());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(this.$.input, 'input', () => {
      this.submitEnabled_ = this.$.input.value.length > 0;
    });
    // Make the element focusable to receive keyboard events.
    this.$.composebox.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid) {
      return;
    }
    this.files_ = this.files_.filter((file) => file.uuid !== e.detail.uuid);
  }

  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;
    if (!files || files.length === 0) {
      return;
    }
    const newFiles: ComposeboxFile[] = [];
    for (let i = 0; i < files.length; i++) {
      const file = files.item(i)!;
      if (file.size === 0 || file.size > this.maxFileSize_) {
        // TODO(crbug.com/422559050): Show error state.
      } else {
        newFiles.push({
          uuid: this.createUuid(),
          name: file.name,
          objectUrl:
              e.target === this.$.imageInput ? URL.createObjectURL(file) : null,
          type: file.type,
        });
        // TODO(crbug.com/422559977): Upload the file.
      }
    }
    this.files_ = this.files_.concat(newFiles);
    // Clear the file input.
    input.value = '';
  }

  protected openImageUpload_() {
    this.$.imageInput.click();
  }

  protected openFileUpload_() {
    this.$.fileInput.click();
  }

  private createUuid(): string {
    return BigInt
        .asUintN(
            64,
            BigInt(`0x${crypto.randomUUID().replace(/-/g, '')}`),
            )
        .toString();
  }

  protected onCancelClick_() {
    if (this.$.input.value.length > 0) {
      this.$.input.value = '';
      // TODO(rtatum@): Send request to handler to clear file cache.
      this.files_ = [];
      this.submitEnabled_ = false;
    } else {
      this.notifySessionAbandoned_();
    }
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.notifySessionAbandoned_();
    }
  }

  private notifySessionAbandoned_() {
    this.pageHandler_.notifySessionAbandoned();
    this.fire('toggle-composebox');
  }

  protected onSubmitClick_() {
    this.submitting_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
