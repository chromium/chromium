// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../event_list/raw_event_details.js';
import '../icons.html.js';
import '../scope_icon.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {localizeScope} from '../event_history.js';
import type {Scope} from '../event_history.js';
import {formatDateLong, formatRelativeDate} from '../tools.js';
import {ShowDirectoryTarget} from '../updater_ui.mojom-webui.js';

import {getCss} from './updater_state_card.css.js';
import {getHtml} from './updater_state_card.html.js';

export class UpdaterStateCardElement extends CrLitElement {
  static get is() {
    return 'updater-state-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      scope: {type: Object},
      version: {type: String},
      inactiveVersions: {type: Array},
      lastChecked: {type: Object},
      lastStarted: {type: Object},
      installPath: {type: String},
    };
  }

  accessor scope: Scope|undefined = undefined;
  accessor version: string|undefined = undefined;
  accessor inactiveVersions: string[] = [];
  accessor lastChecked: Date|null = null;
  accessor lastStarted: Date|null = null;
  accessor installPath: string|undefined = undefined;

  protected headingLabel: string = '';
  protected formattedLastChecked: string = '';
  protected formattedLastCheckedRelative: string = '';
  protected formattedLastStarted: string = '';
  protected formattedLastStartedRelative: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('scope')) {
      this.headingLabel = this.computeHeadingLabel();
    }
    if (changedProperties.has('lastChecked') && this.lastChecked !== null) {
      const {date, relative} = this.computeFormattedLastChecked();
      this.formattedLastChecked = date;
      this.formattedLastCheckedRelative = relative;
    }
    if (changedProperties.has('lastStarted') && this.lastStarted !== null) {
      const {date, relative} = this.computeFormattedLastStarted();
      this.formattedLastStarted = date;
      this.formattedLastStartedRelative = relative;
    }
  }

  private computeHeadingLabel(): string {
    return this.scope !== undefined ? localizeScope(this.scope) : '';
  }

  private computeFormattedLastChecked(): {date: string, relative: string} {
    assert(this.lastChecked !== null);
    return {
      date: formatDateLong(this.lastChecked),
      relative: formatRelativeDate(this.lastChecked),
    };
  }

  private computeFormattedLastStarted(): {date: string, relative: string} {
    assert(this.lastStarted !== null);
    return {
      date: formatDateLong(this.lastStarted),
      relative: formatRelativeDate(this.lastStarted),
    };
  }

  protected onInstallPathClick() {
    BrowserProxyImpl.getInstance().handler.showDirectory(
        this.scope === 'SYSTEM' ? ShowDirectoryTarget.kSystemUpdater :
                                  ShowDirectoryTarget.kUserUpdater);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-state-card': UpdaterStateCardElement;
  }
}

customElements.define(UpdaterStateCardElement.is, UpdaterStateCardElement);
