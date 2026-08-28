// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {FilePath} from '//resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';

import {getCss} from './installed_app_list_item.css.js';
import {getHtml} from './installed_app_list_item.html.js';
import type {IwaDevModeAppInfo} from './iwa_dev.mojom-webui.js';

interface SourceMetadata {
  label: string;
  description: string;
}

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param origin Origin to convert.
 */
function originToText(origin: Origin): string {
  if (origin.host.length === 0) {
    return 'null';
  }
  const url = new URL(`${origin.scheme}://${origin.host}`);
  if (origin.port !== 0) {
    url.port = origin.port.toString();
  }
  return url.origin;
}

/**
 * Converts a mojo representation of `base::FilePath` into a user-readable
 * string.
 * @param filePath File path to convert
 */
function filePathToText(filePath: FilePath): string {
  if (typeof filePath.path === 'string') {
    return filePath.path;
  }
  // On Windows, base::FilePath is represented as a UTF-16 code unit array
  // rather than a string.
  const decoder = new TextDecoder('utf-16');
  return decoder.decode(new Uint16Array(filePath.path));
}

export class InstalledAppListItemElement extends CrLitElement {
  static get is() {
    return 'installed-app-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      sourceMetadata: {type: Object},
      isUpdating: {type: Boolean},
      copied_: {type: Boolean, state: true},
    };
  }

  accessor app: IwaDevModeAppInfo = {
    appId: '',
    webBundleId: '',
    name: '',
    source: {
      proxyOrigin: {
        scheme: '',
        host: '',
        port: 0,
        nonceIfOpaque: null,
      },
    },
    installedVersion: '',
  };

  accessor isUpdating: boolean = false;

  protected accessor sourceMetadata: SourceMetadata = {
    label: '',
    description: '',
  };

  protected accessor copied_: boolean = false;
  private copiedTimeoutId_: number|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.copiedTimeoutId_ !== null) {
      window.clearTimeout(this.copiedTimeoutId_);
      this.copiedTimeoutId_ = null;
    }
  }

  protected isManifestApp_(): boolean {
    return !!this.app.source.updateInfo;
  }

  protected onCopyClick() {
    if (this.app.source.updateInfo) {
      navigator.clipboard.writeText(
          this.app.source.updateInfo.updateManifestUrl);
      this.copied_ = true;
      if (this.copiedTimeoutId_ !== null) {
        window.clearTimeout(this.copiedTimeoutId_);
      }
      this.copiedTimeoutId_ = window.setTimeout(() => {
        this.copied_ = false;
        this.copiedTimeoutId_ = null;
      }, 2000);
    }
  }

  protected onUpdateOptionsClick() {
    this.fire('request-update-options', {app: this.app});
  }

  protected onUpdateClick() {
    this.fire('request-update', {app: this.app});
  }

  protected onUninstallClick() {
    this.fire('request-uninstall', {app: this.app});
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app')) {
      this.sourceMetadata = this.computeSourceMetadata();
    }
  }

  /**
   * Returns the metadata to be displayed for the app's installation source.
   */
  private computeSourceMetadata(): SourceMetadata {
    if (this.app.source.proxyOrigin) {
      return {
        label: 'Dev mode proxy',
        description: originToText(this.app.source.proxyOrigin),
      };
    }
    if (this.app.source.bundlePath) {
      return {
        label: 'Local Bundle',
        description: filePathToText(this.app.source.bundlePath),
      };
    }
    if (this.app.source.updateInfo) {
      const info = this.app.source.updateInfo;
      return {
        label: 'Update manifest',
        description: `${info.updateManifestUrl}`,
      };
    }
    assertNotReached();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'installed-app-list-item': InstalledAppListItemElement;
  }
}

customElements.define(
    InstalledAppListItemElement.is, InstalledAppListItemElement);
