// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/icons.html.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './infobar_internals.mojom-webui.js';
import type {InfoBarEntry, InfoBarType} from './infobar_internals.mojom-webui.js';

export interface InfobarInternalsAppElement {
  $: {
    dropdownButton: CrButtonElement,
    menu: CrActionMenuElement,
    toast: CrToastElement,
  };
}

export class InfobarInternalsAppElement extends CrLitElement {
  static get is() {
    return 'infobar-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      infobars: {type: Array},
      searchQuery: {type: String},
      selectedTypes: {type: Array},
      toastMessage: {type: String},
    };
  }

  protected accessor infobars: InfoBarEntry[] = [];
  protected accessor searchQuery: string = '';
  protected accessor selectedTypes: InfoBarType[] = [];
  protected accessor toastMessage: string = '';

  override connectedCallback() {
    super.connectedCallback();
    browserProxyFactory.getInstance().handler.getInfoBars().then(
        ({infobars}) => this.infobars = infobars);
  }

  protected onDropdownClick() {
    this.$.menu.showAt(this.$.dropdownButton, {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  protected onSearchChanged(e: CustomEvent<string>) {
    this.searchQuery = e.detail;
  }

  protected onSelectionChange(e: Event) {
    const checkbox = e.currentTarget as CrCheckboxElement;
    const typeStr = checkbox.dataset['type'];
    assert(typeStr);

    const typeNum = Number(typeStr);
    assert(!Number.isNaN(typeNum));

    const type = typeNum as InfoBarType;
    this.selectedTypes = checkbox.checked ?
        [...this.selectedTypes, type] :
        this.selectedTypes.filter(selected => selected !== type);
  }

  protected onClearSelectionClick() {
    this.selectedTypes = [];
  }

  protected isSelected(type: InfoBarType): boolean {
    return this.selectedTypes.includes(type);
  }

  protected areAllFilteredSelected(): boolean {
    const filtered = this.getFilteredInfobars();
    if (filtered.length === 0) {
      return false;
    }
    return filtered.every(infobar => this.isSelected(infobar.type));
  }

  protected onSelectAllClick() {
    const filtered = this.getFilteredInfobars();
    if (filtered.length === 0) {
      return;
    }
    if (this.areAllFilteredSelected()) {
      const filteredTypes = new Set(filtered.map(i => i.type));
      this.selectedTypes =
          this.selectedTypes.filter(type => !filteredTypes.has(type));
    } else {
      const newTypes = new Set(this.selectedTypes);
      for (const infobar of filtered) {
        newTypes.add(infobar.type);
      }
      this.selectedTypes = Array.from(newTypes);
    }
  }

  protected getFilteredInfobars(): InfoBarEntry[] {
    const query = this.searchQuery.trim().toLowerCase();
    if (!query) {
      return this.infobars;
    }
    return this.infobars.filter(
        infobar => infobar.name.toLowerCase().includes(query));
  }

  protected getSelectedInfobars(): InfoBarEntry[] {
    return this.infobars.filter(infobar => this.isSelected(infobar.type));
  }

  protected getDropdownLabel(): string {
    const count = this.selectedTypes.length;
    if (count === 0) {
      return 'Select infobars';
    }
    return `${count} ${count === 1 ? 'infobar' : 'infobars'} selected`;
  }

  protected async onTriggerClick(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const typeStr = target.dataset['type'];
    assert(typeStr);

    const typeNum = Number(typeStr);
    assert(!Number.isNaN(typeNum));

    const name = target.dataset['name'] || 'Infobar';

    const type = typeNum as InfoBarType;
    await this.trigger(type, name);
  }

  private async trigger(id: InfoBarType, name: string) {
    const {success} =
        await browserProxyFactory.getInstance().handler.triggerInfoBar(id);
    if (success) {
      this.toastMessage = `Triggered "${name}"`;
    } else {
      this.toastMessage = `Failed to trigger "${name}"`;
      console.warn('Failed to trigger infobar', id);
    }
    this.$.toast.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'infobar-internals-app': InfobarInternalsAppElement;
  }
}

customElements.define(
    InfobarInternalsAppElement.is, InfobarInternalsAppElement);
