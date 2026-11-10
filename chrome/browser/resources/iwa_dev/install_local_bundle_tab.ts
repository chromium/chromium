// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {Code} from '//resources/mojo/mojo/public/mojom/base/error.mojom-webui.js';

import {getCss} from './install_local_bundle_tab.css.js';
import {getHtml} from './install_local_bundle_tab.html.js';
import {IwaDevInstallTabElement} from './install_tab.js';
import type {BrowserProxy} from './iwa_dev.mojom-webui.js';
import {browserProxyFactory} from './iwa_dev.mojom-webui.js';

export class IwaDevInstallLocalBundleTabElement extends
    IwaDevInstallTabElement {
  static get is() {
    return 'iwa-dev-install-local-bundle-tab';
  }

  static override get styles() {
    return [
      super.styles,
      getCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      selectedFileName_: {type: String, state: true},
      isDragOver_: {type: Boolean, state: true},
      errorMessage_: {type: String, state: true},
    };
  }

  accessor disabled: boolean = false;
  protected accessor selectedFileName_: string|null = null;
  protected accessor isDragOver_: boolean = false;
  protected accessor errorMessage_: string = '';
  private isSelecting_: boolean = false;
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.notifyValidChanged();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.handler.clearStagedLocalWebBundle();
  }

  override isValid(): boolean {
    return this.selectedFileName_ !== null;
  }

  override submit() {
    this.fire('request-install-from-local-bundle');
  }

  protected async onDropzoneClick_() {
    if (this.disabled || this.isSelecting_) {
      return;
    }
    this.isSelecting_ = true;
    try {
      this.selectedFileName_ =
          await this.browserProxy_.handler.selectLocalWebBundle();
      this.errorMessage_ = '';
      this.notifyValidChanged();
    } catch (err: unknown) {
      if ((err as {code?: Code})?.code === Code.kCancelled) {
        return;
      }
      this.selectedFileName_ = null;
      this.errorMessage_ =
          (err as {message?: string})?.message || 'Failed to select bundle.';
      this.notifyValidChanged();
    } finally {
      this.isSelecting_ = false;
    }
  }

  protected onDropzoneKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      if (e.repeat) {
        return;
      }
      this.onDropzoneClick_();
    }
  }

  protected onDragover_(e: DragEvent) {
    e.preventDefault();
    e.stopPropagation();
    if (this.disabled ||
        (e.dataTransfer && !e.dataTransfer.types.includes('Files'))) {
      if (e.dataTransfer) {
        e.dataTransfer.dropEffect = 'none';
      }
      return;
    }
    if (!this.isDragOver_) {
      this.isDragOver_ = true;
      this.browserProxy_.handler.notifyDragBundleEnter();
    }
  }

  protected onDragleave_(e: DragEvent) {
    if (this.disabled) {
      return;
    }
    e.preventDefault();
    const dropzone = e.currentTarget;
    if (dropzone instanceof HTMLElement && e.relatedTarget instanceof Node &&
        dropzone.contains(e.relatedTarget)) {
      return;
    }
    this.isDragOver_ = false;
    this.browserProxy_.handler.notifyDragBundleLeave();
  }

  protected async onDrop_(e: DragEvent) {
    e.preventDefault();
    e.stopPropagation();
    this.isDragOver_ = false;
    if (this.disabled ||
        (e.dataTransfer && !e.dataTransfer.types.includes('Files'))) {
      return;
    }
    try {
      this.selectedFileName_ =
          await this.browserProxy_.handler.stageDroppedLocalWebBundle();
      this.errorMessage_ = '';
      this.notifyValidChanged();
    } catch (err: unknown) {
      this.selectedFileName_ = null;
      this.errorMessage_ = (err as {message?: string})?.message ||
          'Failed to stage dropped bundle.';
      this.notifyValidChanged();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-install-local-bundle-tab': IwaDevInstallLocalBundleTabElement;
  }
}

customElements.define(
    IwaDevInstallLocalBundleTabElement.is, IwaDevInstallLocalBundleTabElement);
