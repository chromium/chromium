// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {ActionButtonType, browserProxyFactory, IconType} from './page_action_internals.mojom-webui.js';
import type {ExpandableItem, PageHandlerInterface} from './page_action_internals.mojom-webui.js';

export class PageActionInternalsAppElement extends CrLitElement {
  static get is() {
    return 'page-action-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageActionIcon: {type: Number},
      showChip: {type: Boolean},
      chipText: {type: String},
      messageText: {type: String},
      bubbleIcon: {type: Number},
      actionIcon: {type: Number},
      hasDrawer: {type: Boolean},
      drawerHeading: {type: String},
      drawerItems: {type: Array},
      drawerIcon: {type: Number},
    };
  }

  // Properties bound to UI
  protected accessor pageActionIcon: IconType|null = IconType.kInfo;
  protected accessor showChip: boolean = false;
  protected accessor chipText: string = 'Test Chip Text';
  protected accessor messageText: string =
      'This is a prototype Anchored Message!';
  protected accessor bubbleIcon: IconType|null = IconType.kInfo;
  protected accessor actionIcon: ActionButtonType|null =
      ActionButtonType.kClose;
  protected accessor hasDrawer: boolean = false;
  protected accessor drawerHeading: string = 'Expanded Details';
  protected accessor drawerItems: string[] = ['Item 1 text', 'Item 2 text'];
  protected accessor drawerIcon: IconType|null = IconType.kOrangeAFavicon;

  private pageHandler_: PageHandlerInterface =
      browserProxyFactory.getInstance().handler;

  protected onPageActionIconChange(e: Event) {
    const value = (e.target as HTMLSelectElement).value;
    this.pageActionIcon = value === 'none' ? null : (Number(value) as IconType);
  }

  protected onShowChipCheckedChanged(e: CustomEvent<{value: boolean}>) {
    this.showChip = e.detail.value;
  }

  protected onChipTextValueChanged(e: CustomEvent<{value: string}>) {
    this.chipText = e.detail.value;
  }

  protected onMessageTextValueChanged(e: CustomEvent<{value: string}>) {
    this.messageText = e.detail.value;
  }

  protected onBubbleIconChange(e: Event) {
    const value = (e.target as HTMLSelectElement).value;
    this.bubbleIcon = value === 'none' ? null : (Number(value) as IconType);
  }

  protected onActionIconChange(e: Event) {
    const value = (e.target as HTMLSelectElement).value;
    this.actionIcon =
        value === 'none' ? null : (Number(value) as ActionButtonType);
  }

  protected onHasDrawerCheckedChanged(e: CustomEvent<{value: boolean}>) {
    this.hasDrawer = e.detail.value;
  }

  protected onDrawerHeadingValueChanged(e: CustomEvent<{value: string}>) {
    this.drawerHeading = e.detail.value;
  }

  protected onDrawerIconChange(e: Event) {
    const value = (e.target as HTMLSelectElement).value;
    this.drawerIcon = value === 'none' ? null : (Number(value) as IconType);
  }

  protected onDrawerItemValueChanged(e: CustomEvent<{value: string}>) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const newItems = [...this.drawerItems];
    newItems[index] = e.detail.value;
    this.drawerItems = newItems;
  }

  protected onAddDrawerItemClick() {
    this.drawerItems =
        [...this.drawerItems, `Item ${this.drawerItems.length + 1} text`];
  }

  protected onRemoveDrawerItemClick(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    this.drawerItems = this.drawerItems.filter((_, i) => i !== index);
  }

  protected onShowPageActionClick() {
    this.showPageAction();
  }

  protected onShowAnchoredMessageClick() {
    this.showAnchoredMessage();
  }

  protected onHideClick() {
    this.hidePageAction();
  }

  private showPageAction() {
    this.pageHandler_.showPageAction({
      iconType: this.pageActionIcon,
      chipText: this.showChip ? this.chipText : null,
    });
  }

  private showAnchoredMessage() {
    let expandableContent = null;
    if (this.hasDrawer) {
      const items: ExpandableItem[] =
          this.drawerItems.map(text => ({
                                 text,
                                 iconType: this.drawerIcon,
                               }));
      expandableContent = {
        heading: this.drawerHeading,
        items,
      };
    }

    this.pageHandler_.showAnchoredMessage({
      messageText: this.messageText,
      bubbleIcon: this.bubbleIcon,
      actionIcon: this.actionIcon,
      expandableContent,
    });
  }

  private hidePageAction() {
    this.pageHandler_.hidePageAction();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-internals-app': PageActionInternalsAppElement;
  }
}

customElements.define(
    PageActionInternalsAppElement.is, PageActionInternalsAppElement);
