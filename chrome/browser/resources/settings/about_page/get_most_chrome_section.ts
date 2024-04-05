// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'get-most-chrome-section' defines a settings-section
 * with a link to the "Get the most out of Chrome" page.
 */

import '../settings_page/settings_section.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './get_most_chrome_section.html.js';

const GetMostChromeSectionElementBase = I18nMixin(PolymerElement);

export class GetMostChromeSectionElement extends
    GetMostChromeSectionElementBase {
  static get is() {
    return 'get-most-chrome-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entries_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private entries_: Array<{iconDark: string, iconLight: string, label: string}>;

  override ready() {
    super.ready();
    this.entries_ = [
      {
        iconDark: 'gtmooc_computer_dark.svg',
        iconLight: 'gtmooc_computer.svg',
        label: this.i18n('getTheMostOutOfChromeWorkBetterForYou'),
      },
      {
        iconDark: 'gtmooc_search_dark.svg',
        iconLight: 'gtmooc_search.svg',
        label: this.i18n('getTheMostOutOfChromeSearchHistory'),
      },
      {
        iconDark: 'gtmooc_cookie_dark.svg',
        iconLight: 'gtmooc_cookie.svg',
        label: this.i18n('getTheMostOutOfChromeThirdPartyCookies'),
      },
    ];
  }

  private onGetTheMostOutOfChromeClick_() {
    Router.getInstance().navigateTo(routes.GET_MOST_CHROME);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'get-most-chrome-section': GetMostChromeSectionElement;
  }
}

customElements.define(
    GetMostChromeSectionElement.is, GetMostChromeSectionElement);
