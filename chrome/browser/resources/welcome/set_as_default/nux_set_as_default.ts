// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
// <if expr="is_win">
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
// </if>
import '../shared/step_indicator.js';
import '../strings.m.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {NavigationMixin} from '../navigation_mixin.js';
import {navigateToNextStep} from '../router.js';
import type {DefaultBrowserInfo, StepIndicatorModel} from '../shared/nux_types.js';

import {getCss} from './nux_set_as_default.css.js';
import {getHtml} from './nux_set_as_default.html.js';
import type {NuxSetAsDefaultProxy} from './nux_set_as_default_proxy.js';
import {NuxSetAsDefaultProxyImpl} from './nux_set_as_default_proxy.js';

export interface NuxSetAsDefaultElement {
  $: {
    declineButton: HTMLElement,
  };
}

const NuxSetAsDefaultElementBase =
    WebUiListenerMixinLit(NavigationMixin(CrLitElement));

export class NuxSetAsDefaultElement extends NuxSetAsDefaultElementBase {
  static get is() {
    return 'nux-set-as-default';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      indicatorModel: {type: Object},

      // <if expr="is_win">
      isWin10_: {type: Boolean},
      // </if>
    };
  }

  indicatorModel?: StepIndicatorModel;

  // <if expr="is_win">
  protected isWin10_: boolean = loadTimeData.getBoolean('is_win10');
  // </if>

  private browserProxy_: NuxSetAsDefaultProxy;
  private finalized_: boolean = false;
  navigateToNextStep: Function;

  constructor() {
    super();
    this.subtitle = loadTimeData.getString('setDefaultHeader');
    this.navigateToNextStep = navigateToNextStep;
    this.browserProxy_ = NuxSetAsDefaultProxyImpl.getInstance();
  }

  override firstUpdated() {
    this.addWebUiListener(
        'browser-default-state-changed',
        this.onDefaultBrowserChange_.bind(this));
  }

  override onRouteEnter() {
    this.finalized_ = false;
    this.browserProxy_.recordPageShown();
  }

  override onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAwayThroughBrowserHistory();
  }

  override onRouteUnload() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAway();
  }

  protected onDeclineClick_() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordSkip();
    this.finished_();
  }

  protected onSetDefaultClick_() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordBeginSetDefault();
    this.browserProxy_.setAsDefault();
  }

  /**
   * Automatically navigate to the next onboarding step once default changed.
   */
  private onDefaultBrowserChange_(status: DefaultBrowserInfo) {
    if (status.isDefault) {
      this.browserProxy_.recordSuccessfullySetDefault();
      // Triggers toast in the containing welcome-app.
      this.dispatchEvent(new CustomEvent(
          'default-browser-change', {bubbles: true, composed: true}));
      this.finished_();
      return;
    }

    // <if expr="is_macosx">
    // On Mac OS, we do not get a notification when the default browser changes.
    // This will fake the notification.
    window.setTimeout(() => {
      this.browserProxy_.requestDefaultBrowserState().then(
          this.onDefaultBrowserChange_.bind(this));
    }, 100);
    // </if>
  }

  private finished_() {
    this.finalized_ = true;
    this.navigateToNextStep();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'nux-set-as-default': NuxSetAsDefaultElement;
  }
}

customElements.define(NuxSetAsDefaultElement.is, NuxSetAsDefaultElement);
