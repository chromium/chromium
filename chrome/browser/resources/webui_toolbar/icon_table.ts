// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconHandle, IconUpdate} from './toolbar_ui_api_data_model.mojom-webui.js';

interface IconInfo {
  urlOrName: string;
  isUrl: boolean;
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

  constructor() {
    const fallbackIcon =
        // A 1x1 transparent PNG generated via <canvas>.
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAA' +
        'fFcSJAAAADUlEQVR4AWJiYGBgAAAAAP//XRcpzQAAAAZJREFUAwAADwADJDd9' +
        '6QAAAABJRU5ErkJggg==';

    // Handle 0 has the semantics of mapping to a transparent icon.
    this.icons_.set(0n, {
      urlOrName: fallbackIcon,
      isUrl: true,
    });
  }

  applyUpdates(updates: IconUpdate[]): void {
    for (const update of updates) {
      if (update.iconUrlOrName) {
        this.icons_.set(update.handleId, {
          urlOrName: update.iconUrlOrName,
          isUrl: update.iconIsUrl,
        });
      } else {
        this.icons_.delete(update.handleId);
      }
    }
  }

  // If the icon with given handle has been registered as having a URL,
  // returns that URL. Otherwise returns undefined.
  //
  // If defined, you'll want to set it on a --cr-icon-image attribute for
  // a <cr-icon-button>, or use it as a mask image.
  getIconUrl(handle: IconHandle): string|undefined {
    const maybeIcon = this.icons_.get(handle.handleId);
    return maybeIcon && maybeIcon.isUrl ? maybeIcon.urlOrName : undefined;
  }

  // If the icon with given handle has been registered as being a name in
  // an iconset, return that name. Otherwise returns undefined.
  // If defined, you'll probably want to set it as an iron-icon attribute.
  getIconName(handle: IconHandle): string|undefined {
    const maybeIcon = this.icons_.get(handle.handleId);
    return maybeIcon && !maybeIcon.isUrl ? maybeIcon.urlOrName : undefined;
  }
}
