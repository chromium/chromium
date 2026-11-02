// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {State} from './managed_user_profile_notice_browser_proxy.js';
import {getCss} from './managed_user_profile_notice_state_refresh.css.js';
import {getHtml} from './managed_user_profile_notice_state_refresh.html.js';

export interface ManagedUserProfileNoticeStateRefreshElement {
  $: {
    title: HTMLElement,
    subtitle: HTMLElement,
  };
}

const ManagedUserProfileNoticeStateRefreshElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

/**
 * Shows the processing, success, timeout or error state, picking the icon,
 * title and subtitle based on `state`.
 *
 * The error and long processing strings are received via WebUI listeners.
 * These are only sent by the OIDC enrollment flow, which always shows the
 * processing state first, so the element exists when they are received.
 */
export class ManagedUserProfileNoticeStateRefreshElement extends
    ManagedUserProfileNoticeStateRefreshElementBase {
  static get is() {
    return 'managed-user-profile-notice-state-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Number},
      errorTitle_: {type: String},
      errorSubtitle_: {type: String},
      processingSubtitle_: {type: String},
    };
  }

  accessor state: State = State.PROCESSING;
  private accessor errorTitle_: string = '';
  private accessor errorSubtitle_: string = '';
  private accessor processingSubtitle_: string =
      loadTimeData.getString('processingSubtitle');

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'on-state-changed-to-error',
        (errorTitle: string, errorSubtitle: string) => {
          this.errorTitle_ = errorTitle;
          this.errorSubtitle_ = errorSubtitle;
        });

    this.addWebUiListener('on-long-processing', () => {
      this.processingSubtitle_ = this.i18n('longProcessingSubtitle');
    });
  }

  protected isState_(state: State): boolean {
    return this.state === state;
  }

  protected getTitle_(): string {
    switch (this.state) {
      case State.PROCESSING:
        return '';
      case State.SUCCESS:
        return this.i18n('successTitle');
      case State.TIMEOUT:
        return this.i18n('timeoutTitle');
      case State.ERROR:
        return this.errorTitle_;
      default:
        assertNotReached();
    }
  }

  protected getSubtitle_(): string {
    switch (this.state) {
      case State.PROCESSING:
        return this.processingSubtitle_;
      case State.SUCCESS:
        return this.i18n('successSubtitle');
      case State.TIMEOUT:
        return this.i18n('timeoutSubtitle');
      case State.ERROR:
        return this.errorSubtitle_;
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-state-refresh':
        ManagedUserProfileNoticeStateRefreshElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeStateRefreshElement.is,
    ManagedUserProfileNoticeStateRefreshElement);
