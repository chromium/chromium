// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './content_setting_pattern_source.js';

import {PageHandler, PageHandlerRemote} from './privacy_sandbox_internals.mojom-webui.js';

class DataLoader {
  pageHandler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.pageHandler = handler;
  }

  async load() {
    const cookieParent =
        document.querySelector<HTMLElement>('#cookie-content-settings')!;
    const cookieSettings = await this.pageHandler.getCookieSettings();
    cookieSettings.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      cookieParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdMetadataParent =
        document.querySelector<HTMLElement>('#tpcd-metadata-grants')!;
    const tpcdMetadataGrants = await this.pageHandler.getTpcdMetadataGrants();
    tpcdMetadataGrants.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdMetadataParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdHeuristicsParent =
        document.querySelector<HTMLElement>('#tpcd-heuristics-grants')!;
    const tpcdHeuristicsGrants =
        await this.pageHandler.getTpcdHeuristicsGrants();
    tpcdHeuristicsGrants.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdHeuristicsParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdSupportParent =
        document.querySelector<HTMLElement>('#tpcd-support')!;
    const tpcdSupport = await this.pageHandler.getTpcdSupport();
    tpcdSupport.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdSupportParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const loader = new DataLoader(PageHandler.getRemote());
  loader.load();
});
