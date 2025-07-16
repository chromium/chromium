// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import type {PageHandlerRemote} from '../composebox.mojom-webui.js';
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
    fileUploadButton: CrIconButtonElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: CrIconButtonElement,
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
      files_: {type: Object},
      imageFileTypes_: {type: String},
      inputsDisabled_: {type: Boolean},
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
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  protected accessor inputsDisabled_: boolean = false;
  protected accessor submitEnabled_: boolean = false;
  protected accessor submitting_: boolean = false;
  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private pageHandler_: PageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();

  private composeboxCloseByEscape_: boolean =
      loadTimeData.getBoolean('composeboxCloseByEscape');

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
      this.submitEnabled_ = this.$.input.value.trim().length > 0;
    });
    this.$.input.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('files_')) {
      this.computeInputsDisabled_();
    }
  }

  private computeInputsDisabled_() {
    this.inputsDisabled_ = this.files_.size >= this.maxFileCount_;
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }
    const newFileMap: Map<UnguessableToken, ComposeboxFile> =
        new Map(this.files_.entries());
    newFileMap.delete(e.detail.uuid);
    this.files_ = newFileMap;
    this.pageHandler_.deleteFile(e.detail.uuid);
  }

  protected async onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;
    if (!files || files.length === 0 ||
        this.files_.size >= this.maxFileCount_) {
      return;
    }
    const newFileMap: Map<UnguessableToken, ComposeboxFile> =
        new Map(this.files_.entries());
    for (let i = 0; i < files.length; i++) {
      const file = files.item(i)!;
      if (file.size === 0 || file.size > this.maxFileSize_) {
        // TODO(crbug.com/422559050): Show error state.
      } else {
        const fileBuffer = await file.arrayBuffer();
        if (!file.type.includes('pdf') && !file.type.includes('image')) {
          return;
        }

        const bigBuffer:
            BigBuffer = {bytes: Array.from(new Uint8Array(fileBuffer))};

        const {token} = await this.pageHandler_.addFile(
            {
              fileName: file.name,
              mimeType: file.type,
              selectionTime: new Date(),
            },
            bigBuffer);
        newFileMap.set(token, {
          uuid: token,
          name: file.name,
          objectUrl: input === this.$.imageInput ? URL.createObjectURL(file) :
                                                   null,
          type: file.type,
        });
      }
    }
    this.files_ = newFileMap;
    // Clear the file input.
    input.value = '';
    this.$.input.focus();
  }

  protected openImageUpload_() {
    this.$.imageInput.click();
  }

  protected openFileUpload_() {
    this.$.fileInput.click();
  }

  protected onCancelClick_() {
    if (this.$.input.value.trim().length > 0) {
      this.$.input.value = '';
      this.files_ = new Map();
      this.submitEnabled_ = false;
      this.pageHandler_.clearFiles();
    } else {
      this.notifySessionAbandoned_();
    }
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey && this.submitEnabled_) {
      this.onSubmitClick_(e);
    }
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' && this.composeboxCloseByEscape_) {
      this.notifySessionAbandoned_();
    }
  }

  private notifySessionAbandoned_() {
    this.pageHandler_.notifySessionAbandoned();
    this.fire('toggle-composebox');
  }

  protected onSubmitClick_(e: KeyboardEvent|MouseEvent) {
    this.pageHandler_.submitQuery(
        this.$.input.value.trim(), (e as MouseEvent).button || 0, e.altKey,
        e.ctrlKey, e.metaKey, e.shiftKey);
    this.submitting_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
