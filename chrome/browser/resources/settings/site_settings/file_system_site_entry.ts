// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-entry' is an element representing a single origin's
 * permission grant(s), granted via the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './file_system_site_entry_item.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './file_system_site_entry.html.js';
import {OriginFileSystemGrants} from './file_system_site_list.js';

export interface FileSystemSiteEntryElement {
  $: {
    collapseChild: IronCollapseElement,
    dropdownButton: CrExpandButtonElement,
  };
}

export class FileSystemSiteEntryElement extends PolymerElement {
  static get is() {
    return 'file-system-site-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * An Object representing an origin and its associated permission grants.
       */
      grantsPerOrigin: Object,
    };
  }
  grantsPerOrigin: OriginFileSystemGrants;
}
declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-entry': FileSystemSiteEntryElement;
  }
}

customElements.define(
    FileSystemSiteEntryElement.is, FileSystemSiteEntryElement);
