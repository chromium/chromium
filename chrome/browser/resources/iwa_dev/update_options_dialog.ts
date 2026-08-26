// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ChannelMetadata, IwaDevModeAppInfo, UpdateManifest, VersionEntry} from './iwa_dev.mojom-webui.js';
import {getCss as getSharedCss} from './shared_style.css.js';
import {getCss} from './update_options_dialog.css.js';
import {getHtml} from './update_options_dialog.html.js';

export interface UpdateOptionsSavedEventDetail {
  app: IwaDevModeAppInfo;
  selectedChannel?: string;
  pinnedVersion?: string|null;
  allowDowngrades?: boolean;
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
      currentPinnedVersion: {type: String},
      currentAllowDowngrades: {type: Boolean},
      isFetching_: {type: Boolean, state: true},
      fetchError_: {type: String, state: true},
      channels_: {type: Array, state: true},
      selectedChannel_: {type: String, state: true},
      versions_: {type: Array, state: true},
      selectedPinnedVersion_: {type: String, state: true},
      selectedAllowDowngrades_: {type: Boolean, state: true},
    };
  }

  accessor app: IwaDevModeAppInfo = {
    appId: '',
    webBundleId: '',
    name: '',
    source: {},
    installedVersion: '',
  };

  accessor currentPinnedVersion: string|null = null;
  accessor currentAllowDowngrades: boolean = false;

  protected accessor isFetching_: boolean = true;
  protected accessor fetchError_: string = '';
  protected accessor channels_: ChannelMetadata[] = [];
  protected accessor selectedChannel_: string = '';
  protected accessor versions_: VersionEntry[] = [];
  protected accessor selectedPinnedVersion_: string = '';
  protected accessor selectedAllowDowngrades_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.selectedChannel_ = this.getCurrentChannel_();
    this.selectedPinnedVersion_ = this.getCurrentPinnedVersion_() || '';
    this.selectedAllowDowngrades_ = this.currentAllowDowngrades;
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

  protected onPinnedVersionInput_(e: Event) {
    this.selectedPinnedVersion_ = (e.target as HTMLInputElement).value;
  }

  protected onClearPinnedVersionClick_() {
    this.selectedPinnedVersion_ = '';
    this.shadowRoot.querySelector<HTMLElement>('#pinnedVersionInput')?.focus();
  }

  protected onAllowDowngradesChange_(e: CustomEvent<boolean>) {
    this.selectedAllowDowngrades_ = e.detail;
  }

  protected getCurrentChannel_(): string {
    return this.app.source.updateInfo?.updateChannel || 'default';
  }

  protected getCurrentPinnedVersion_(): string|null {
    return this.currentPinnedVersion;
  }

  protected getChannelPlaceholder_(): string {
    return this.isFetching_ ? 'Loading channels...' : 'Select or enter channel';
  }

  protected getVersionPlaceholder_(): string {
    return this.isFetching_ ? 'Loading versions...' : 'Select or enter version';
  }

  protected isSaveDisabled_(): boolean {
    if (this.isFetching_) {
      return true;
    }
    return !this.hasChannelChange_() && !this.hasPinnedVersionChange_() &&
        !this.hasAllowDowngradesChange_();
  }

  protected onSaveClick_() {
    if (this.isSaveDisabled_()) {
      return;
    }

    const detail: UpdateOptionsSavedEventDetail = {app: this.app};

    if (this.hasChannelChange_()) {
      detail.selectedChannel = this.selectedChannel_.trim();
    }

    if (this.hasPinnedVersionChange_()) {
      const version = this.selectedPinnedVersion_.trim();
      detail.pinnedVersion = version.length === 0 ? null : version;
    }

    if (this.hasAllowDowngradesChange_()) {
      detail.allowDowngrades = this.selectedAllowDowngrades_;
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
          this.versions_ = result.success.versions || [];
        }
      },
    });
  }

  private hasChannelChange_(): boolean {
    const channel = this.selectedChannel_.trim();
    return channel.length > 0 && channel !== this.getCurrentChannel_();
  }

  private hasPinnedVersionChange_(): boolean {
    const version = this.selectedPinnedVersion_.trim();
    const current = this.getCurrentPinnedVersion_() || '';
    return version !== current;
  }

  private hasAllowDowngradesChange_(): boolean {
    return this.selectedAllowDowngrades_ !== this.currentAllowDowngrades;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-update-options-dialog': IwaDevUpdateOptionsDialogElement;
  }
}

customElements.define(
    IwaDevUpdateOptionsDialogElement.is, IwaDevUpdateOptionsDialogElement);
