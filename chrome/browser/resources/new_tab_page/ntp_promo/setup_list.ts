// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './ntp_promo_icons.html.js';
import './setup_list_item.js';
import '../modules/icons.html.js';
import '../modules/info_dialog.js';
import '../modules/module_header.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit} from '../i18n_setup.js';
import type {ModuleHeaderElement} from '../modules/module_header.js';
import type {NtpPromoClientCallbackRouter, NtpPromoHandlerInterface, Promo} from '../ntp_promo.mojom-webui.js';

import {NtpPromoProxyImpl} from './ntp_promo_proxy.js';
import {getCss} from './setup_list.css.js';
import {getHtml} from './setup_list.html.js';

export interface SetupListElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElement,
    promos: HTMLElement,
  };
}

export class SetupListElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'setup-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      maxPromos: {type: Number, attribute: true, useDefault: true},
      maxCompletedPromos: {type: Number, attribute: true, useDefault: true},

      completedPromos_: {type: Array},

      eligiblePromos_: {type: Array},

      showInfoDialog_: {type: Boolean},

      allowFaviconServerFallback_: {type: Boolean},
    };
  }

  accessor maxPromos: number = 0;
  accessor maxCompletedPromos: number = 0;

  protected accessor completedPromos_: Promo[] = [];
  protected accessor eligiblePromos_: Promo[] = [];
  protected accessor showInfoDialog_: boolean = false;

  private handler_: NtpPromoHandlerInterface;
  private callbackRouter_: NtpPromoClientCallbackRouter;
  private listenerIds_: number[] = [];
  private notifiedShown_: boolean = false;

  constructor() {
    super();
    this.handler_ = NtpPromoProxyImpl.getInstance().getHandler();
    this.callbackRouter_ = NtpPromoProxyImpl.getInstance().getCallbackRouter();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(this.callbackRouter_.setPromos.addListener(
        this.onSetPromos.bind(this)));
    this.handler_.requestPromos();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.listenerIds_) {
      this.callbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
  }

  onSetPromos(eligible: Promo[], completed: Promo[]) {
    this.completedPromos_ = completed.slice(0, this.maxCompletedPromos);
    const maxEligible = this.maxPromos - this.completedPromos_.length;
    this.eligiblePromos_ = eligible.slice(0, maxEligible);
    const hasPromos =
        this.completedPromos_.length !== 0 || this.eligiblePromos_.length !== 0;
    const readyEvent = new CustomEvent('module-ready', {
      bubbles: true,
      composed: true,
      detail: hasPromos,
    });
    this.dispatchEvent(readyEvent);

    if (!this.notifiedShown_) {
      this.notifiedShown_ = true;
      const eligible: string[] = this.eligiblePromos_.map(promo => promo.id);
      const completed: string[] = this.completedPromos_.map(promo => promo.id);
      this.handler_.onPromosShown(eligible, completed);
    }
  }

  protected onPromoClick_(e: CustomEvent) {
    const promoId = e.detail;
    assert(promoId, 'Entry should never have empty promo ID.');
    this.handler_.onPromoClicked(promoId);
  }

  protected onDisableButtonClick_() {
    this.handler_.disableSetupList();
    const disableEvent = new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: this.i18nRecursive(
            '', 'modulesSetupListDisableToastMessage', 'modulesSetupListTitle'),
        restoreCallback: () => this.handler_.undisableSetupList(),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onDismissButtonClick_() {
    this.handler_.snoozeSetupList();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: this.i18nRecursive(
            '', 'modulesSetupListDismissToastMessage', 'modulesSetupListTitle'),
        restoreCallback: () => this.handler_.unsnoozeSetupList(),
      },
    }));
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'setup-list': SetupListElement;
  }
}

customElements.define(SetupListElement.is, SetupListElement);
