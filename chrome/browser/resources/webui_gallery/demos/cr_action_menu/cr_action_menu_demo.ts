// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../demo.css.js';

import {AnchorAlignment, CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_action_menu_demo.html.js';

interface CrActionMenuDemoElement {
  $: {
    menu: CrActionMenuElement,
    minMaxContainer: HTMLDivElement,
    anchorAlignmentDemo: HTMLButtonElement,
  };
}

class CrActionMenuDemoElement extends PolymerElement {
  static get is() {
    return 'cr-action-menu-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      alignmentOptions_: Array,
      customAlignmentX_: {
        type: String,
        value: 'CENTER',
      },
      customAlignmentY_: {
        type: String,
        value: 'CENTER',
      },
      statusText_: String,
    };
  }

  private alignmentOptions_: Array<keyof typeof AnchorAlignment> = [
    'BEFORE_START',
    'AFTER_START',
    'CENTER',
    'BEFORE_END',
    'AFTER_END',
  ];
  private customAlignmentX_: keyof typeof AnchorAlignment;
  private customAlignmentY_: keyof typeof AnchorAlignment;

  private onShowAnchoredMenuClick_(event: MouseEvent) {
    this.$.menu.showAt(event.target as HTMLElement);
  }

  private onContextMenu_(event: MouseEvent) {
    event.preventDefault();
    this.$.menu.close();
    this.$.menu.showAtPosition({top: event.clientY, left: event.clientX});
  }

  private onShowMinMaxMenu_(event: MouseEvent) {
    const minMaxContainerRect = this.$.minMaxContainer.getBoundingClientRect();
    const config = {
      minX: minMaxContainerRect.left,
      maxX: minMaxContainerRect.right,
      minY: minMaxContainerRect.top,
      maxY: minMaxContainerRect.bottom,
    };
    this.$.menu.showAt(event.target as HTMLElement, config);
  }

  private onAnchorAlignmentDemoClick_() {
    this.$.menu.showAt(this.$.anchorAlignmentDemo, {
      anchorAlignmentX: AnchorAlignment[this.customAlignmentX_],
      anchorAlignmentY: AnchorAlignment[this.customAlignmentY_],
    });
  }

  private isSelectedAlignment_(
      selectedAlignment: AnchorAlignment, option: AnchorAlignment) {
    return selectedAlignment === option;
  }
}

export const tagName = CrActionMenuDemoElement.is;

customElements.define(CrActionMenuDemoElement.is, CrActionMenuDemoElement);
