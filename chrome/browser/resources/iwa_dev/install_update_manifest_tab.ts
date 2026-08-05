// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import {IwaDevInstallTabElement} from './install_tab.js';
import {getCss} from './install_update_manifest_tab.css.js';
import {getHtml} from './install_update_manifest_tab.html.js';
import type {ChannelMetadata, UpdateManifest, VersionEntry} from './iwa_dev.mojom-webui.js';

export const PLACEHOLDER_URL =
    'https://github.com/chromeos/iwa-sink/releases/latest/download/update.json';

export const MIN_FETCH_DELAY_MS = 750;

export class IwaDevInstallUpdateManifestTabElement extends
    IwaDevInstallTabElement {
  static get is() {
    return 'iwa-dev-install-update-manifest-tab';
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
      url_: {type: String, state: true},
      urlError_: {type: String, state: true},
      isFetching_: {type: Boolean, state: true},
      isManifestFetched_: {type: Boolean, state: true},
      selectedVersion_: {type: String, state: true},
      selectedChannel_: {type: String, state: true},
      versions_: {type: Array, state: true},
      channels_: {type: Array, state: true},
    };
  }

  accessor disabled: boolean = false;
  protected accessor url_: string = '';
  protected accessor urlError_: string = '';
  protected accessor isFetching_: boolean = false;
  protected accessor isManifestFetched_: boolean = false;
  protected accessor selectedVersion_: string = '';
  protected accessor selectedChannel_: string = '';
  protected accessor versions_: VersionEntry[] = [];
  protected accessor channels_: ChannelMetadata[] = [];

  override isValid(): boolean {
    return this.isManifestFetched_;
  }

  override submit() {
    if (!this.isManifestFetched_) {
      return;
    }
    const versionEntry =
        this.versions_.find(v => v.version === this.selectedVersion_)!;
    this.fire('request-install-from-update-manifest', {
      webBundleUrl: versionEntry.src,
      updateInfo: {
        updateManifestUrl: this.url_,
        updateChannel: this.selectedChannel_,
      },
    });
  }

  private onManifestFetched_(result: {
    success?: UpdateManifest,
    error?: string,
  }) {
    this.isFetching_ = false;
    if (result.error) {
      this.urlError_ = result.error;
      this.isManifestFetched_ = false;
    } else if (!result.success?.versions.length) {
      this.urlError_ = 'No valid version entries found in update manifest.';
      this.isManifestFetched_ = false;
    } else {
      this.versions_ = [...result.success.versions].sort(
          (a, b) => this.compareVersions_(b.version, a.version));
      this.channels_ = result.success.channels || [];
      this.selectedVersion_ = this.versions_[0]!.version;
      this.selectedChannel_ = this.channels_[0]?.channel || '';
      this.isManifestFetched_ = true;
    }
    this.notifyValidChanged();
  }

  protected async onFetchClick_() {
    this.urlError_ = '';
    if (!this.url_) {
      return;
    }
    if (!this.isValidUrl_(this.url_)) {
      this.urlError_ = 'Please enter a valid URL.';
      return;
    }

    this.isFetching_ = true;

    // Keep the fetching state shown for at least MIN_FETCH_DELAY_MS to give
    // visual feedback.
    const [result] = await Promise.all([
      new Promise<{success?: UpdateManifest, error?: string}>(resolve => {
        this.fire('request-parse-update-manifest-from-url', {
          url: this.url_,
          callback: resolve,
        });
      }),
      new Promise(resolve => setTimeout(resolve, MIN_FETCH_DELAY_MS)),
    ]);

    this.onManifestFetched_(result);
  }

  protected onUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.url_ = e.detail.value;
    this.urlError_ = '';
    if (this.isManifestFetched_) {
      this.isManifestFetched_ = false;
      this.notifyValidChanged();
    }
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Tab' && !this.url_) {
      e.preventDefault();
      this.url_ = PLACEHOLDER_URL;
      return;
    }
    if (e.key === 'Enter' && !this.isFetching_) {
      this.onFetchClick_();
    }
  }

  protected onSelectKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.isManifestFetched_) {
      this.submit();
    }
  }

  protected onVersionChange_(e: Event) {
    this.selectedVersion_ = (e.target as HTMLSelectElement).value;
  }

  protected onChannelChange_(e: Event) {
    this.selectedChannel_ = (e.target as HTMLSelectElement).value;
  }

  private compareVersions_(v1: string, v2: string): number {
    return v1.localeCompare(
        v2, undefined, {numeric: true, sensitivity: 'base'});
  }

  private isValidUrl_(url: string): boolean {
    try {
      new URL(url);
      return true;
    } catch {
      return false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-install-update-manifest-tab':
        IwaDevInstallUpdateManifestTabElement;
  }
}

customElements.define(
    IwaDevInstallUpdateManifestTabElement.is,
    IwaDevInstallUpdateManifestTabElement);
