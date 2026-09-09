// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './action_row.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {CriticalActionItem, PageHandlerInterface} from './critical_actions.mojom-webui.js';
import {browserProxyFactory, CriticalActionsError} from './critical_actions.mojom-webui.js';

export class CriticalActionsAppElement extends CrLitElement {
  static get is() {
    return 'critical-actions-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries_: {type: Array},
      totalEntries_: {type: Number},
      pageIndex_: {type: Number},
      pageSize_: {type: Number},
      searchQuery_: {type: String},
      actionTypeFilter_: {type: Number},
      isFeatureEnabled_: {type: Boolean},
    };
  }

  protected accessor entries_: CriticalActionItem[] = [];
  protected accessor totalEntries_: number = 0;
  protected accessor pageIndex_: number = 0;
  protected accessor pageSize_: number = 25;
  protected accessor searchQuery_: string = '';
  protected accessor actionTypeFilter_: number|null = null;
  protected accessor isFeatureEnabled_: boolean = true;

  private handler_: PageHandlerInterface =
      browserProxyFactory.getInstance().handler;

  override connectedCallback() {
    super.connectedCallback();
    this.loadData_();
  }

  private async loadData_() {
    const searchFilter = this.searchQuery_.trim() || null;
    const response = await this.handler_.getCriticalActions(
        this.pageIndex_, this.pageSize_, searchFilter, this.actionTypeFilter_);
    if (response.result.error !== undefined) {
      if (response.result.error === CriticalActionsError.kFeatureDisabled) {
        this.isFeatureEnabled_ = false;
      }
      this.entries_ = [];
      this.totalEntries_ = 0;
      return;
    }

    assert(response.result.list);
    this.isFeatureEnabled_ = true;
    this.entries_ = response.result.list.entries;
    this.totalEntries_ = response.result.list.totalEntries;
    this.pageIndex_ = response.result.list.pageIndex;
    this.pageSize_ = response.result.list.pageSize;
  }

  protected computeTotalPages_(): number {
    return Math.max(1, Math.ceil(this.totalEntries_ / this.pageSize_));
  }

  protected getPaginationInfo_(): string {
    const start =
        this.totalEntries_ === 0 ? 0 : this.pageIndex_ * this.pageSize_ + 1;
    const end =
        Math.min((this.pageIndex_ + 1) * this.pageSize_, this.totalEntries_);
    const currentPage = this.pageIndex_ + 1;
    const totalPages = this.computeTotalPages_();
    return `Showing ${start}–${end} of ${this.totalEntries_} entries (Page ${
        currentPage} of ${totalPages})`;
  }

  protected isFirstPage_(): boolean {
    return this.pageIndex_ === 0;
  }

  protected isLastPage_(): boolean {
    return this.pageIndex_ === this.computeTotalPages_() - 1;
  }

  protected onSearchInput_(e: Event) {
    const target = e.target as HTMLInputElement;
    this.searchQuery_ = target.value;
    this.pageIndex_ = 0;
    this.loadData_();
  }

  protected onActionTypeChange_(e: Event) {
    const target = e.target as HTMLSelectElement;
    const val = parseInt(target.value, 10);
    this.actionTypeFilter_ = val === -1 ? null : val;
    this.pageIndex_ = 0;
    this.loadData_();
  }

  protected onPageSizeChange_(e: Event) {
    const target = e.target as HTMLSelectElement;
    this.pageSize_ = parseInt(target.value, 10);
    this.pageIndex_ = 0;
    this.loadData_();
  }

  protected onRefreshClick_() {
    this.loadData_();
  }

  protected async onClearAllClick_() {
    if (window.confirm(
            'Are you sure you want to clear all critical actions?')) {
      await this.handler_.clearAllCriticalActions();
      this.pageIndex_ = 0;
      this.loadData_();
    }
  }

  protected async onDeleteEntry_(e: CustomEvent<string>) {
    const id = e.detail;
    if (id && window.confirm(`Delete critical action ${id}?`)) {
      await this.handler_.deleteCriticalAction(id);
      this.loadData_();
    }
  }

  protected onFirstPageClick_() {
    this.pageIndex_ = 0;
    this.loadData_();
  }

  protected onPrevPageClick_() {
    if (this.isFirstPage_()) {
      return;
    }
    this.pageIndex_--;
    this.loadData_();
  }

  protected onNextPageClick_() {
    if (this.isLastPage_()) {
      return;
    }
    this.pageIndex_++;
    this.loadData_();
  }

  protected onLastPageClick_() {
    this.pageIndex_ = Math.max(0, this.computeTotalPages_() - 1);
    this.loadData_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'critical-actions-app': CriticalActionsAppElement;
  }
}

customElements.define(CriticalActionsAppElement.is, CriticalActionsAppElement);
