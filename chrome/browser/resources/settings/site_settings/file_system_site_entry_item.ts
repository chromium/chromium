// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-entry-item' is an element representing a single
 * permission grant for a given origin, granted via the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './file_system_site_entry_item.html.js';
import type {FileSystemGrant} from './file_system_site_list.js';

const FileSystemSiteEntryItemElementBase = BaseMixin(PolymerElement);

export interface FileSystemSiteEntryItemElement {
  $: {
    removeGrant: HTMLElement,
  };
}

export class FileSystemSiteEntryItemElement extends
    FileSystemSiteEntryItemElementBase {
  static get is() {
    return 'file-system-site-entry-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * An Object representing an origin and its associated permission grants.
       */
      grant: Object,
    };
  }

  grant: FileSystemGrant;

  private getClassForListItem_(): string {
    return this.grant.isDirectory ? 'icon-folder-open' : 'icon-file';
  }

  private onRemoveGrantClick_() {
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
