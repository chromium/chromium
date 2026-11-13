// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './unexportable_keys_internals.mojom-webui.js';
import type {UnexportableKeyInfo} from './unexportable_keys_internals.mojom-webui.js';

type SortKey = 'wrappedKey'|'algorithm'|'keyTag'|'creationTime';

interface ColumnDef {
  key: SortKey;
  label: string;
  getValue: (item: UnexportableKeyInfo) => string | Date;
}

export interface UnexportableKeysInternalsAppElement {
  $: {
    deleteErrorToast: CrToastElement,
  };
}

export class UnexportableKeysInternalsAppElement extends CrLitElement {
  static get is() {
    return 'unexportable-keys-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      unexportableKeysInfo_: {type: Array},
      sortColumn_: {type: String},
      sortReverse_: {type: Boolean},
      tagPrefix_: {type: String},
    };
  }

  protected readonly columns_: ColumnDef[] = [
    {
      key: 'wrappedKey',
      label: 'Wrapped Key',
      getValue: (item) => item.wrappedKey,
    },
    {key: 'algorithm', label: 'Algorithm', getValue: (item) => item.algorithm},
    {key: 'keyTag', label: 'Key Tag', getValue: (item) => item.keyTag},
    {
      key: 'creationTime',
      label: 'Creation Time',
      getValue: (item) => item.creationTime,
    },
  ];

  protected accessor unexportableKeysInfo_: UnexportableKeyInfo[] = [];
  protected accessor tagPrefix_: string = '';
  private accessor sortColumn_: SortKey = 'creationTime';
  private accessor sortReverse_: boolean = true;

  private browserProxy_ = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.updateKeysList_();
  }

  protected async onDeleteKeyClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const keyId =
        this.unexportableKeysInfo_[Number(currentTarget.dataset['index'])]!
            .keyId;
    const {success} = await this.browserProxy_.handler.deleteKeys([keyId]);
    this.onDeleteFinished_(success);
  }

  protected onTagPrefixInput_(e: Event) {
    this.tagPrefix_ = (e.target as HTMLInputElement).value;
  }

  // Returns the listed keys whose tag starts with `tagPrefix_`. An empty prefix
  // only matches keys without a tag, so that a single click can't delete the
  // keys of all channels and user data directories.
  protected getMatchingKeys_(): UnexportableKeyInfo[] {
    return this.unexportableKeysInfo_.filter(
        item => this.tagPrefix_ ? item.keyTag.startsWith(this.tagPrefix_) :
                                  !item.keyTag);
  }

  protected async onDeleteMatchingKeysClick_() {
    const keys = this.getMatchingKeys_();
    if (!window.confirm(`Are you sure you want to delete ${keys.length} ${
            keys.length === 1 ? 'key' : 'keys'}?`)) {
      return;
    }
    const {success} =
        await this.browserProxy_.handler.deleteKeys(keys.map(key => key.keyId));
    this.onDeleteFinished_(success);
  }

  protected onSortClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const sortColumn = target.dataset['sortKey'] as SortKey;
    if (this.sortColumn_ === sortColumn) {
      this.sortReverse_ = !this.sortReverse_;
    } else {
      this.sortColumn_ = sortColumn;
      this.sortReverse_ = false;
    }
    this.sortKeys_();
  }

  protected onSortKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onSortClick_(e);
    }
  }

  protected getSortAttribute_(column: SortKey): string|undefined {
    if (this.sortColumn_ !== column) {
      return undefined;
    }
    return this.sortReverse_ ? 'descending' : 'ascending';
  }

  private async updateKeysList_() {
    const {keys} = await this.browserProxy_.handler.getUnexportableKeysInfo();
    this.unexportableKeysInfo_ = keys;
    this.sortKeys_();
  }

  private onDeleteFinished_(success: boolean) {
    if (!success) {
      this.$.deleteErrorToast.show();
    } else if (this.$.deleteErrorToast.open) {
      // Hide the toast if it was shown before but this time the deletion
      // succeeded.
      this.$.deleteErrorToast.hide();
    }
    this.updateKeysList_();
  }

  private sortKeys_() {
    const getValue =
        this.columns_.find(col => col.key === this.sortColumn_)?.getValue;

    if (!getValue) {
      return;
    }

    this.unexportableKeysInfo_ =
        [...this.unexportableKeysInfo_].sort((lhs, rhs) => {
          const a = getValue(lhs);
          const b = getValue(rhs);
          const sign = this.sortReverse_ ? -1 : 1;
          return sign * (a > b ? 1 : a < b ? -1 : 0);
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'unexportable-keys-internals-app': UnexportableKeysInternalsAppElement;
  }
}

customElements.define(
    UnexportableKeysInternalsAppElement.is,
    UnexportableKeysInternalsAppElement);
