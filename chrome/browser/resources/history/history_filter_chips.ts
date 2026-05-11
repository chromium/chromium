/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// <if expr="_google_chrome">
import './internal/icons.html.js';
// </if>

import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserServiceImpl} from './browser_service.js';
import {getCss} from './history_filter_chips.css.js';
import {getHtml} from './history_filter_chips.html.js';

export class HistoryFilterChipsElement extends CrLitElement {
  static get is() {
    return 'history-filter-chips';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      userVisits: {type: Boolean},
      actorVisits: {type: Boolean},
    };
  }

  // These values are used to filter the history query; true indicates that
  // results for the respective value should be included.
  private accessor userVisits: boolean = true;
  private accessor actorVisits: boolean = true;

  protected isUserSelected(): boolean {
    return this.userVisits && !this.actorVisits;
  }

  protected isActorSelected(): boolean {
    return this.actorVisits && !this.userVisits;
  }

  protected onUserVisitsClick_() {
    this.userVisits = true;
    this.actorVisits = !this.actorVisits;

    this.recordMetrics_(this.userVisits, this.actorVisits);
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected onActorVisitsClick_() {
    this.actorVisits = true;
    this.userVisits = !this.userVisits;

    this.recordMetrics_(this.userVisits, this.actorVisits);
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected recordMetrics_(newUserState: boolean, newActorState: boolean) {
    let action = '';
    if (newUserState && newActorState) {
      action = 'HistoryPage_ShowAllEnabled';
    } else if (newUserState && !newActorState) {
      action = 'HistoryPage_ShowUserOnlyEnabled';
    } else if (!newUserState && newActorState) {
      action = 'HistoryPage_ShowActorOnlyEnabled';
    }

    if (action !== '') {
      BrowserServiceImpl.getInstance().recordAction(action);
    }
  }

  protected getUserVisitsIcon_(): string {
    return this.isUserSelected() ? 'cr:check' : 'cr:person';
  }

  protected getActorVisitsIcon_(): string {
    if (this.isActorSelected()) {
      return 'cr:check';
    }
    // <if expr="_google_chrome">
    return 'history-internal:screensaver-auto';
    // </if>
    // <if expr="not _google_chrome">
    return '';
    // </if>
  }

  private fireChange_(userVisits: boolean, actorVisits: boolean) {
    this.fire('filter-changed', {userVisits, actorVisits});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-filter-chips': HistoryFilterChipsElement;
  }
}

customElements.define(HistoryFilterChipsElement.is, HistoryFilterChipsElement);
