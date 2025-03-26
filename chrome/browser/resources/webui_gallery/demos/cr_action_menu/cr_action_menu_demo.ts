// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_action_menu_demo.css.js';
import {getHtml} from './cr_action_menu_demo.html.js';

type AnchorAlignmentKey = keyof typeof AnchorAlignment;

export interface CrActionMenuDemoElement {
  $: {
    menu: CrActionMenuElement,
    minMaxContainer: HTMLElement,
    anchorAlignmentDemo: HTMLButtonElement,
  };
}

export class CrActionMenuDemoElement extends CrLitElement {
  static get is() {
    return 'cr-action-menu-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      alignmentOptions_: {type: Array},
      customAlignmentX_: {type: String},
      customAlignmentY_: {type: String},
    };
  }

  protected accessor alignmentOptions_: AnchorAlignmentKey[] = [
    'BEFORE_START',
    'AFTER_START',
    'CENTER',
    'BEFORE_END',
    'AFTER_END',
  ];
  protected accessor customAlignmentX_: AnchorAlignmentKey = 'CENTER';
  protected accessor customAlignmentY_: AnchorAlignmentKey = 'CENTER';

  protected onShowAnchoredMenuClick_(event: MouseEvent) {
    this.$.menu.showAt(event.target as HTMLElement);
  }

  protected onContextMenu_(event: MouseEvent) {
    event.preventDefault();
    this.$.menu.close();
    this.$.menu.showAtPosition({top: event.clientY, left: event.clientX});
  }

  protected onShowMinMaxMenu_(event: MouseEvent) {
    const minMaxContainerRect = this.$.minMaxContainer.getBoundingClientRect();
    const config = {
      minX: minMaxContainerRect.left,
      maxX: minMaxContainerRect.right,
      minY: minMaxContainerRect.top,
      maxY: minMaxContainerRect.bottom,
    };
    this.$.menu.showAt(event.target as HTMLElement, config);
  }

  protected onAnchorAlignmentDemoClick_() {
    this.$.menu.showAt(this.$.anchorAlignmentDemo, {
      anchorAlignmentX: AnchorAlignment[this.customAlignmentX_],
      anchorAlignmentY: AnchorAlignment[this.customAlignmentY_],
    });
  }

  protected isSelectedAlignment_(
      selectedAlignment: AnchorAlignmentKey, option: AnchorAlignmentKey) {
    return selectedAlignment === option;
  }

  protected onCustomAlignmentXChanged_(e: Event) {
    this.customAlignmentX_ =
        (e.target as HTMLSelectElement).value as AnchorAlignmentKey;
  }

  protected onCustomAlignmentYChanged_(e: Event) {
    this.customAlignmentY_ =
        (e.target as HTMLSelectElement).value as AnchorAlignmentKey;
  }
}

export const tagName = CrActionMenuDemoElement.is;

customElements.define(CrActionMenuDemoElement.is, CrActionMenuDemoElement);
