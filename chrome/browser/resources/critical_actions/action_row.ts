// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './action_row.css.js';
import {getHtml} from './action_row.html.js';
import type {CriticalActionItem} from './critical_actions.mojom-webui.js';

const DEFAULT_CRITICAL_ACTION_ITEM: CriticalActionItem = {
  criticalActionId: '',
  timestampRaw: 0n,
  timestampStr: '',
  visitId: 0n,
  conversationId: '',
  actorTaskId: '',
  actionType: 0,
  actionTypeStr: '',
  actionSource: 0,
  actionSourceStr: '',
  label: '',
  tooltip: '',
  url: '',
  metadata: '',
};

export class CriticalActionRowElement extends CrLitElement {
  static get is() {
    return 'critical-action-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      item: {type: Object},
      isExpanded_: {type: Boolean},
    };
  }

  accessor item: CriticalActionItem = DEFAULT_CRITICAL_ACTION_ITEM;
  protected accessor isExpanded_: boolean = false;

  protected onToggleMetadataClick_() {
    this.isExpanded_ = !this.isExpanded_;
  }

  protected onDeleteClick_() {
    this.fire('delete-entry', this.item.criticalActionId);
  }

  protected formatJson_(): string {
    const metadata = this.item.metadata || '';
    try {
      return JSON.stringify(JSON.parse(metadata), null, 2);
    } catch {
      return metadata;
    }
  }

  protected getActionTypeClass_(): string {
    const type = (this.item.actionTypeStr || '').toLowerCase();
    switch (type) {
      case 'formfill':
      case 'download':
      case 'settingchange':
        return `type-${type}`;
      case 'credentialaccess':
      case 'googlepasswordmanager':
      case 'federatedlogin':
      case 'credentialsotp':
        return 'type-credential';
      default:
        return 'type-unknown';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'critical-action-row': CriticalActionRowElement;
  }
}

customElements.define(CriticalActionRowElement.is, CriticalActionRowElement);
