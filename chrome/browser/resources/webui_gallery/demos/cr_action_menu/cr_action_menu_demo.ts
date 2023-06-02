// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../demo.css.js';

import {AnchorAlignment, CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_action_menu_demo.html.js';

interface CrActionMenuDemoElement {
  $: {
    menu: CrActionMenuElement,
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
      enableCustomContextMenu_: Boolean,
      numberInputs_: Array,
      showAtPositionConfig_: Object,
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
  private enableCustomContextMenu_: boolean = false;
  private numberInputs_: Array<keyof ShowAtPositionConfig> = [
    'top',
    'left',
    'width',
    'height',
    'minX',
    'minY',
    'maxX',
    'maxY',
  ];
  private showAtPositionConfig_: ShowAtPositionConfig = {top: 0, left: 0};
  private statusText_: string;

  override ready() {
    super.ready();
    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
  }

  private onAlignmentChangedX_(event: Event) {
    const select = event.target as HTMLSelectElement;
    if (!select.value) {
      delete this.showAtPositionConfig_.anchorAlignmentX;
      return;
    }
    const key = select.value as keyof typeof AnchorAlignment;
    this.showAtPositionConfig_.anchorAlignmentX = AnchorAlignment[key];
  }

  private onAlignmentChangedY_(event: Event) {
    const select = event.target as HTMLSelectElement;
    if (!select.value) {
      delete this.showAtPositionConfig_.anchorAlignmentY;
      return;
    }
    const key = select.value as keyof typeof AnchorAlignment;
    this.showAtPositionConfig_.anchorAlignmentY = AnchorAlignment[key];
  }

  private onContextMenu_(event: MouseEvent) {
    if (!this.enableCustomContextMenu_) {
      return;
    }

    event.preventDefault();
    this.$.menu.close();
    this.$.menu.showAtPosition({top: event.clientY, left: event.clientX});
  }

  private onMenuItem1Click_() {
    this.$.menu.close();
    this.statusText_ = 'Clicked item 1';
  }

  private onMenuItem2Click_() {
    this.$.menu.close();
    this.statusText_ = 'Clicked item 2';
  }

  private onShowAnchoredMenuClick_(event: MouseEvent) {
    this.$.menu.showAt(event.target as HTMLElement);
  }

  private onShowMenuClick_() {
    this.$.menu.showAtPosition(this.showAtPositionConfig_);
  }

  private onNumberInputChanged_(
      event: DomRepeatEvent<keyof ShowAtPositionConfig>) {
    const crInput = event.target as CrInputElement;
    const key = event.model.item;
    // Inputs have values that are strings, so convert them to numbers or
    // delete them from the config if undefined.
    if (crInput.value) {
      (this.showAtPositionConfig_[key] as number) = Number(crInput.value);
    } else {
      delete this.showAtPositionConfig_[key];
    }
  }
}

export const tagName = CrActionMenuDemoElement.is;

customElements.define(CrActionMenuDemoElement.is, CrActionMenuDemoElement);
