// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconHandle, IconUpdate} from './toolbar_ui_api_data_model.mojom-webui.js';
import {IconType} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface IconInfo {
  urlOrName: string;
  type: IconType;
}

export class IconTable {
  private static instance_?: IconTable;

  static getInstance(): IconTable {
    if (IconTable.instance_ === undefined) {
      IconTable.instance_ = new IconTable();
    }
    return IconTable.instance_;
  }

  private icons_: Map<bigint, IconInfo> = new Map();

  private fallbackIcon_: IconInfo = {
    urlOrName:
        // A 1x1 transparent PNG generated via <canvas>.
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAA' +
        'fFcSJAAAADUlEQVR4AWJiYGBgAAAAAP//XRcpzQAAAAZJREFUAwAADwADJDd9' +
        '6QAAAABJRU5ErkJggg==',
    type:
            // Make it a mask URL so it can work with cr-icon-button.
        IconType.kMaskUrl,
  };

  constructor() {
    // Handle 0 has the semantics of mapping to a transparent icon.
    this.icons_.set(0n, this.fallbackIcon_);
  }

  applyUpdates(updates: IconUpdate[]): void {
    for (const update of updates) {
      if (update.iconUrlOrName) {
        this.icons_.set(update.handleId, {
          urlOrName: update.iconUrlOrName,
          type: update.iconType,
        });
      } else {
        this.icons_.delete(update.handleId);
      }
    }
  }

  reset(): void {
    this.icons_.clear();
  }

  // If the icon with given handle has been registered as having a URL for
  // a single-color mask image, returns that URL. Otherwise returns undefined.
  //
  // If defined, you'll want to set it on a --cr-icon-image attribute for
  // a <cr-icon-button>, or use it as a mask image.
  getIconMaskUrl(handle: IconHandle): string|undefined {
    const maybeIcon = this.icons_.get(handle.handleId);
    return maybeIcon && maybeIcon.type === IconType.kMaskUrl ?
        maybeIcon.urlOrName :
        undefined;
  }

  // If the icon with given handle has been registered as being a name in
  // an iconset, return that name. Otherwise returns undefined.
  // If defined, you'll probably want to set it as an iron-icon attribute.
  getIconName(handle: IconHandle): string|undefined {
    const maybeIcon = this.icons_.get(handle.handleId);
    return maybeIcon && maybeIcon.type === IconType.kIconSet ?
        maybeIcon.urlOrName :
        undefined;
  }

  // Returns full icon info for a handle, substituting in the fallback icon
  // if it's missing.
  getIconInfo(handle: IconHandle): IconInfo {
    const maybeIcon = this.icons_.get(handle.handleId);
    return maybeIcon ?? this.fallbackIcon_;
  }
}
