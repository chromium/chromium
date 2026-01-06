// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog.js';
import './filter_dialog_footer.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getKnownApps} from '../../known_apps.js';

import {getCss} from './app_dialog.css.js';
import {getHtml} from './app_dialog.html.js';

export class AppDialogElement extends CrLitElement {
  static get is() {
    return 'app-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      anchorElement: {type: Object},
      search: {type: String},
      pendingSelections: {type: Object},
      initialSelections: {type: Object},
    };
  }

  accessor anchorElement: HTMLElement|null = null;
  accessor search = '';
  accessor pendingSelections = new Set<string>();
  accessor initialSelections = new Set<string>();
  protected displayedApps: string[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('initialSelections')) {
      this.pendingSelections = new Set(this.initialSelections);
      this.search = '';
    }

    if (changedProperties.has('search') ||
        changedProperties.has('pendingSelections')) {
      this.displayedApps = this.computeDisplayedApps();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.shadowRoot.querySelector<HTMLElement>('.filter-menu-input')?.focus();
  }

  private computeDisplayedApps(): string[] {
    const apps = getKnownApps();
    const predefinedApps = Array.from(apps.keys()).filter(appName => {
      return appName.toLowerCase().includes(this.search.toLowerCase());
    });
    const customApps = Array.from(this.pendingSelections).filter(appName => {
      return !apps.has(appName) &&
          appName.toLowerCase().includes(this.search.toLowerCase());
    });
    const appCandidates = new Set([...predefinedApps, ...customApps]);
    if (this.search && !apps.has(this.search) &&
        !this.pendingSelections.has(this.search)) {
      appCandidates.add(this.search);
    }
    return Array.from(appCandidates);
  }

  protected onAppSearchInput(e: InputEvent) {
    this.search = (e.target as HTMLInputElement).value;
  }

  protected onAppSearchKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      if (input.value) {
        this.pendingSelections.add(input.value);
        this.search = '';
        this.requestUpdate();
      }
    }
  }

  protected onCheckedChanged(e: Event) {
    const checkbox = e.target as HTMLInputElement;
    const appName = checkbox.dataset['appName']!;
    checkbox.checked ? this.pendingSelections.add(appName) :
                       this.pendingSelections.delete(appName);
    this.requestUpdate();
  }

  protected onAppApplyClick() {
    this.fire('filter-change', this.pendingSelections);
  }

  protected onCancelClick() {
    this.fire('close');
  }

  protected onClose() {
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-dialog': AppDialogElement;
  }
}

customElements.define(AppDialogElement.is, AppDialogElement);
