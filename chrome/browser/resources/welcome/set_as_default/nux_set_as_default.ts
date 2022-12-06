// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
// <if expr="is_win">
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
// </if>
import '../shared/animations.css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateToNextStep, NavigationMixin} from '../navigation_mixin.js';
import {DefaultBrowserInfo, StepIndicatorModel} from '../shared/nux_types.js';

import {getTemplate} from './nux_set_as_default.html.js';
import {NuxSetAsDefaultProxy, NuxSetAsDefaultProxyImpl} from './nux_set_as_default_proxy.js';

export interface NuxSetAsDefaultElement {
  $: {
    declineButton: HTMLElement,
  };
}

const NuxSetAsDefaultElementBase =
    WebUiListenerMixin(NavigationMixin(PolymerElement));

/** @polymer */
export class NuxSetAsDefaultElement extends NuxSetAsDefaultElementBase {
  static get is() {
    return 'nux-set-as-default';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      indicatorModel: Object,

      // <if expr="is_win">
      isWin10: {
        type: Boolean,
        value: loadTimeData.getBoolean('is_win10'),
      },
      // </if>

      subtitle: {
        type: String,
        value: loadTimeData.getString('setDefaultHeader'),
      },
    };
  }

  private browserProxy_: NuxSetAsDefaultProxy;
  private finalized_: boolean = false;
  navigateToNextStep: Function;
  indicatorModel?: StepIndicatorModel;

  constructor() {
    super();
    this.navigateToNextStep = navigateToNextStep;
    this.browserProxy_ = NuxSetAsDefaultProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

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

  private onDeclineClick_() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordSkip();
    this.finished_();
  }

  private onSetDefaultClick_() {
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
