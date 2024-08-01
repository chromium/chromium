// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import './images/icons.html.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './header_menu.html.js';

export interface HeaderMenuElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class HeaderMenuElement extends PolymerElement {
  static get is() {
    return 'header-menu';
  }

  static get template() {
    return getTemplate();
  }

  showAt(element: HTMLElement) {
    const rect = element.getBoundingClientRect();
    const verticalOffsetPx = 4;
    this.$.menu.get().showAt(element, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      top: rect.bottom + verticalOffsetPx,
      left: rect.left,
    });
  }

  close() {
    this.$.menu.get().close();
  }

  private onRenameClick_() {
    this.close();
    this.dispatchEvent(new CustomEvent('rename-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onSeeAllClick_() {
    this.close();
    this.dispatchEvent(new CustomEvent('see-all-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onDeleteClick_() {
    this.close();
    this.dispatchEvent(new CustomEvent('delete-click', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'header-menu': HeaderMenuElement;
  }
}

customElements.define(HeaderMenuElement.is, HeaderMenuElement);
