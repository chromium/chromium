// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon/cr_iconset.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconsetElement} from '//resources/cr_elements/cr_icon/cr_iconset.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_icons_demo.css.js';
import {getHtml} from './cr_icons_demo.html.js';

export class CrIconsDemoElement extends CrLitElement {
  static get is() {
    return 'cr-icons-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      crIcons_: {type: Array},
      iconColor_: {type: String},
      iconSize_: {type: String},
      icons_: {type: Array},
    };
  }

  protected crIcons_: string[] = [
    'icon-arrow-back',  'icon-arrow-dropdown', 'icon-cancel',
    'icon-clear',       'icon-copy-content',   'icon-delete-gray',
    'icon-edit',        'icon-folder-open',    'icon-picture-delete',
    'icon-expand-less', 'icon-expand-more',    'icon-external',
    'icon-more-vert',   'icon-refresh',        'icon-search',
    'icon-settings',    'icon-visibility',     'icon-visibility-off',
    'subpage-arrow',
  ];
  protected iconColor_: string = '#000000';
  protected iconSize_: string = '24';
  protected icons_: string[] = [];

  override firstUpdated() {
    function getIconNames(iconset: CrIconsetElement) {
      return Array.from(iconset.querySelectorAll('g[id]'))
          .map((el: Element) => {
            return `${iconset.name}:${el.id}`;
          });
    }

    // Iconsets are appended to the document's head element when they are
    // imported.
    const crIconsSet = document.head.querySelector<CrIconsetElement>(
        'cr-iconset[name=cr]');
    assert(crIconsSet);
    this.icons_.push(...getIconNames(crIconsSet));

    const cr20IconsSet = document.head.querySelector<CrIconsetElement>(
        'cr-iconset[name=cr20]');
    assert(cr20IconsSet);
    this.icons_.push(...getIconNames(cr20IconsSet));

    this.requestUpdate();
  }

  protected onIconColorInput_(e: Event) {
    const color = (e.target as HTMLInputElement).value;
    this.iconColor_ = color;
  }

  protected onIconSizeChanged_(e: CustomEvent<{value: string}>) {
    this.iconSize_ = e.detail.value;
  }
}

export const tagName = CrIconsDemoElement.is;

customElements.define(CrIconsDemoElement.is, CrIconsDemoElement);
