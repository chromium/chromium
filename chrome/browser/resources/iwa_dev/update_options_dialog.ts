// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ChannelMetadata, IwaDevModeAppInfo, UpdateManifest} from './iwa_dev.mojom-webui.js';
import {getCss as getSharedCss} from './shared_style.css.js';
import {getCss} from './update_options_dialog.css.js';
import {getHtml} from './update_options_dialog.html.js';

export interface UpdateOptionsSavedEventDetail {
  app: IwaDevModeAppInfo;
  selectedChannel?: string;
}

export interface IwaDevUpdateOptionsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class IwaDevUpdateOptionsDialogElement extends CrLitElement {
  static get is() {
    return 'iwa-dev-update-options-dialog';
  }

  static override get styles() {
    return [
      getSharedCss(),
      getCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      isFetching_: {type: Boolean, state: true},
      fetchError_: {type: String, state: true},
      channels_: {type: Array, state: true},
      selectedChannel_: {type: String, state: true},
    };
  }

  accessor app: IwaDevModeAppInfo = {
    appId: '',
    webBundleId: '',
    name: '',
    source: {},
    installedVersion: '',
  };

  protected accessor isFetching_: boolean = true;
  protected accessor fetchError_: string = '';
  protected accessor channels_: ChannelMetadata[] = [];
  protected accessor selectedChannel_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.selectedChannel_ = this.getCurrentChannel_();
  }

  protected onCrDialogOpen_() {
    assert(this.app.source.updateInfo?.updateManifestUrl);
    this.fetchManifest_(this.app.source.updateInfo.updateManifestUrl);
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onChannelInput_(e: Event) {
    this.selectedChannel_ = (e.target as HTMLInputElement).value;
  }

  protected getCurrentChannel_(): string {
    return this.app.source.updateInfo?.updateChannel || 'default';
  }

  protected getChannelPlaceholder_(): string {
    return this.isFetching_ ? 'Loading channels...' : 'Select or enter channel';
  }

  protected isSaveDisabled_(): boolean {
    if (this.isFetching_) {
      return true;
    }
    return !this.hasChannelChange_();
  }

  protected onSaveClick_() {
    if (this.isSaveDisabled_()) {
      return;
    }

    const detail: UpdateOptionsSavedEventDetail = {app: this.app};

    if (this.hasChannelChange_()) {
      detail.selectedChannel = this.selectedChannel_.trim();
    }

    this.fire('update-options-saved', detail);
    this.$.dialog.close();
  }

  private fetchManifest_(url: string) {
    this.isFetching_ = true;
    this.fetchError_ = '';
    this.fire('request-parse-update-manifest-from-url', {
      url,
      callback: (result: {success?: UpdateManifest, error?: string}) => {
        if (!this.$.dialog.open) {
          return;
        }
        this.isFetching_ = false;
        if (result.error) {
          this.fetchError_ =
              'Failed to fetch suggestions from update manifest.';
        } else if (result.success) {
          this.channels_ = result.success.channels || [];
        }
      },
    });
  }

  private hasChannelChange_(): boolean {
    const channel = this.selectedChannel_.trim();
    return channel.length > 0 && channel !== this.getCurrentChannel_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-update-options-dialog': IwaDevUpdateOptionsDialogElement;
  }
}

customElements.define(
    IwaDevUpdateOptionsDialogElement.is, IwaDevUpdateOptionsDialogElement);
