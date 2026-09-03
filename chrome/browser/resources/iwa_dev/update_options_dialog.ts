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

export function isValidIwaVersion(version: string): boolean {
  return /^(0|[1-9]\d*)(\.(0|[1-9]\d*)){0,3}$/.test(version) &&
      version.split('.').every(p => Number(p) <= 4294967295);
}

export function isValidUpdateChannel(channel: string): boolean {
  return channel.length > 0 && channel.isWellFormed();
}

export interface IwaDevUpdateOptionsDialogElement {
  $: {
    channelInput: HTMLInputElement,
    dialog: CrDialogElement,
    pinnedVersionInput: HTMLInputElement,
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
      fetchError_: {type: String, state: true},
      channels_: {type: Array, state: true},
      selectedChannel_: {type: String, state: true},
      versions_: {type: Array, state: true},
      selectedPinnedVersion_: {type: String, state: true},
      selectedAllowDowngrades_: {type: Boolean, state: true},
      channelError_: {type: String, state: true},
      pinnedVersionError_: {type: String, state: true},
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

  protected accessor fetchError_: string = '';
  protected accessor channels_: ChannelMetadata[] = [];
  protected accessor selectedChannel_: string = '';
  protected accessor versions_: VersionEntry[] = [];
  protected accessor selectedPinnedVersion_: string = '';
  protected accessor selectedAllowDowngrades_: boolean = false;
  protected accessor channelError_: string = '';
  protected accessor pinnedVersionError_: string = '';

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
    this.channelError_ = '';
  }

  protected onPinnedVersionInput_(e: Event) {
    this.selectedPinnedVersion_ = (e.target as HTMLInputElement).value;
    this.pinnedVersionError_ = '';
  }

  protected onClearPinnedVersionClick_() {
    this.selectedPinnedVersion_ = '';
    this.pinnedVersionError_ = '';
    this.$.pinnedVersionInput.focus();
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

  protected isSaveDisabled_(): boolean {
    return !!this.channelError_ || !!this.pinnedVersionError_ ||
        (!this.hasChannelChange_() && !this.hasPinnedVersionChange_() &&
         !this.hasAllowDowngradesChange_());
  }

  protected onSaveClick_() {
    if (this.isSaveDisabled_()) {
      return;
    }

    this.channelError_ = '';
    this.pinnedVersionError_ = '';
    const channel = this.selectedChannel_.trim();
    const version = this.selectedPinnedVersion_.trim();
    const detail: UpdateOptionsSavedEventDetail = {app: this.app};
    let hasError = false;

    if (this.hasChannelChange_()) {
      if (channel.length === 0) {
        this.channelError_ = 'Channel cannot be empty.';
        hasError = true;
      } else if (!isValidUpdateChannel(channel)) {
        this.channelError_ = 'Invalid channel format.';
        hasError = true;
      } else {
        detail.selectedChannel = channel;
      }
    }

    if (this.hasPinnedVersionChange_()) {
      if (version.length > 0) {
        if (!isValidIwaVersion(version)) {
          this.pinnedVersionError_ = 'Invalid version format.';
          hasError = true;
        } else {
          detail.pinnedVersion = version;
        }
      } else {
        detail.pinnedVersion = null;
      }
    }

    if (hasError) {
      if (this.channelError_) {
        this.$.channelInput.focus();
      } else if (this.pinnedVersionError_) {
        this.$.pinnedVersionInput.focus();
      }
      return;
    }

    if (this.hasAllowDowngradesChange_()) {
      detail.allowDowngrades = this.selectedAllowDowngrades_;
    }

    this.fire('update-options-saved', detail);
    this.$.dialog.close();
  }

  private fetchManifest_(url: string) {
    this.fetchError_ = '';
    this.fire('request-parse-update-manifest-from-url', {
      url,
      callback: (result: {success?: UpdateManifest, error?: string}) => {
        if (!this.$.dialog.open) {
          return;
        }
        if (result.error) {
          this.fetchError_ =
              'Failed to fetch suggestions from update manifest.';
        } else if (result.success) {
          this.channels_ = result.success.channels || [];
          this.versions_ = [...(result.success.versions || [])].sort(
              (a, b) => this.compareVersions_(b.version, a.version));
        }
      },
    });
  }

  private compareVersions_(v1: string, v2: string): number {
    return v1.localeCompare(
        v2, undefined, {numeric: true, sensitivity: 'base'});
  }

  private hasChannelChange_(): boolean {
    const channel = this.selectedChannel_.trim();
    return channel !== this.getCurrentChannel_();
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
