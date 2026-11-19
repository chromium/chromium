// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-entry-item' is an element representing a single
 * permission grant for a given origin, granted via the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './file_system_site_entry_item.css.js';
import {getHtml} from './file_system_site_entry_item.html.js';
import type {FileSystemGrant} from './file_system_site_list.js';

export interface FileSystemSiteEntryItemElement {
  $: {
    removeGrant: HTMLElement,
  };
}

export class FileSystemSiteEntryItemElement extends CrLitElement {
  static get is() {
    return 'file-system-site-entry-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * An Object representing a permission grant for an origin.
       */
      grant: {type: Object},
    };
  }

  accessor grant: FileSystemGrant = {
    isDirectory: false,
    displayName: '',
    filePath: '',
  };

  protected getClassForListItem_(): string {
    return this.grant.isDirectory ? 'icon-folder-open' : 'icon-file';
  }

  protected onRemoveGrantClick_() {
    this.fire('revoke-grant', this.grant);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-entry-item': FileSystemSiteEntryItemElement;
  }
}

customElements.define(
    FileSystemSiteEntryItemElement.is, FileSystemSiteEntryItemElement);
