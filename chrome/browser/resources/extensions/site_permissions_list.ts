// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';
import './shared_style.js';
import './shared_vars.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFaviconUrl} from './url_util.js';

class ExtensionsSitePermissionsListElement extends PolymerElement {
  static get is() {
    return 'site-permissions-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      header: String,
      sites: Array,
    };
  }

  header: string;
  sites: Array<string>;

  private hasSites_(): boolean {
    return !!this.sites.length;
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }
}

customElements.define(
    ExtensionsSitePermissionsListElement.is,
    ExtensionsSitePermissionsListElement);
