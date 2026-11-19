// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-entry' is an element representing a single origin's
 * permission grant(s), granted via the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../site_favicon.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {getCss} from './file_system_site_entry.css.js';
import {getHtml} from './file_system_site_entry.html.js';
import type {OriginFileSystemGrants} from './file_system_site_list.js';

export class FileSystemSiteEntryElement extends CrLitElement {
  static get is() {
    return 'file-system-site-entry';
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
       * An Object representing an origin and its associated permission grants.
       */
      grantsPerOrigin: {type: Object},
    };
  }

  accessor grantsPerOrigin: OriginFileSystemGrants = {
    origin: '',
    viewGrants: [],
    editGrants: [],
  };

  /**
   * Navigates to the details page for a given origin.
   */
  protected onNavigateToDetailsPageClick_() {
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS,
        new URLSearchParams('site=' + this.grantsPerOrigin.origin));
  }

  protected onRemoveGrantsClick_() {
    this.fire('revoke-grants', this.grantsPerOrigin);
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-entry': FileSystemSiteEntryElement;
  }
}

customElements.define(
    FileSystemSiteEntryElement.is, FileSystemSiteEntryElement);
