// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../demo.css.js';

import {assert} from '//resources/js/assert.js';
import {IronIconsetSvgElement} from '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_icons_demo.html.js';

class CrIconsDemoElement extends PolymerElement {
  static get is() {
    return 'cr-icons-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      crIcons_: Array,
      iconColor_: String,
      iconSize_: String,
      ironIcons_: Array,
    };
  }

  private crIcons_: string[] = [
    'icon-arrow-back',  'icon-arrow-dropdown', 'icon-cancel',
    'icon-clear',       'icon-copy-content',   'icon-delete-gray',
    'icon-edit',        'icon-folder-open',    'icon-picture-delete',
    'icon-expand-less', 'icon-expand-more',    'icon-external',
    'icon-more-vert',   'icon-refresh',        'icon-search',
    'icon-settings',    'icon-visibility',     'icon-visibility-off',
    'subpage-arrow',
  ];
  private iconColor_: string = '#000000';
  private iconSize_: string = '24';
  private ironIcons_: string[] = [];

  override ready() {
    super.ready();

    // Iconsets are appended to the document's head element when they are
    // imported.
    const crIconsSet = document.head.querySelector<IronIconsetSvgElement>(
        'iron-iconset-svg[name=cr]');
    assert(crIconsSet);
    this.push('ironIcons_', ...crIconsSet.getIconNames());

    const cr20IconsSet = document.head.querySelector<IronIconsetSvgElement>(
        'iron-iconset-svg[name=cr20]');
    assert(cr20IconsSet);
    this.push('ironIcons_', ...cr20IconsSet.getIconNames());
  }

  private onIconColorInput_(e: Event) {
    const color = (e.target as HTMLInputElement).value;
    this.iconColor_ = color;
  }
}

export const tagName = CrIconsDemoElement.is;

customElements.define(CrIconsDemoElement.is, CrIconsDemoElement);
