// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AppMode} from './managed_user_profile_notice_browser_proxy.js';
import {getCss} from './managed_user_profile_notice_disclosure_refresh.css.js';
import {getHtml} from './managed_user_profile_notice_disclosure_refresh.html.js';

export interface ManagedUserProfileNoticeDisclosureRefreshElement {
  $: {
    avatar: HTMLImageElement,
    title: HTMLElement,
    subtitle: HTMLElement,
  };
}

const ManagedUserProfileNoticeDisclosureRefreshElementBase =
    I18nMixinLit(CrLitElement);

export class ManagedUserProfileNoticeDisclosureRefreshElement extends
    ManagedUserProfileNoticeDisclosureRefreshElementBase {
  static get is() {
    return 'managed-user-profile-notice-disclosure-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showEnterpriseBadge: {type: Boolean},
      pictureUrl: {type: String},
      title: {type: String},
      subtitle: {type: String},
      appMode: {
        type: String,
        reflect: true,
        attribute: 'app-mode',
      },
      revampEnabled_: {
        type: Boolean,
        reflect: true,
        attribute: 'revamp-enabled',
      },
      isDarkMode_: {type: Boolean},
    };
  }

  accessor showEnterpriseBadge: boolean = false;
  accessor pictureUrl: string = '';
  override accessor title: string = '';
  accessor subtitle: string = '';
  accessor appMode: AppMode = AppMode.FIRST_RUN;
  protected accessor revampEnabled_: boolean =
      loadTimeData.getBoolean('isFirstRunDesktopRevampEnabled');
  protected accessor isDarkMode_: boolean = false;

  private disableAnimations_: boolean =
      loadTimeData.getBoolean('disableAnimations');
  private darkModeListener_: (e: MediaQueryListEvent) => void;
  private matchMedia_: MediaQueryList;

  constructor() {
    super();
    this.matchMedia_ = window.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e) => {
      this.isDarkMode_ = e.matches;
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);
  }

  protected getAnimationUrl_(): string {
    return this.isDarkMode_ ?
        'animations/avatar_sign_in_celebration_dark.json' :
        'animations/avatar_sign_in_celebration.json';
  }

  protected shouldShowAnimations_(): boolean {
    return this.appMode === AppMode.FIRST_RUN && this.revampEnabled_;
  }

  protected shouldDisableAnimations_(): boolean {
    return this.disableAnimations_;
  }

  override firstUpdated() {
    const titleElement = this.shadowRoot.querySelector<HTMLElement>('.title');
    assert(titleElement);
    titleElement.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-disclosure-refresh':
        ManagedUserProfileNoticeDisclosureRefreshElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeDisclosureRefreshElement.is,
    ManagedUserProfileNoticeDisclosureRefreshElement);
