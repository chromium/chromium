// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/images/icons.html.js';
import '../strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ProductSpecificationsDisclosureVersion} from './../shopping_service.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

interface DisclosureItem {
  icon: string;
  text: string;
}

const DisclosureAppElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class DisclosureAppElement extends DisclosureAppElementBase {
  static get is() {
    return 'product-specifications-disclosure-app';
  }

  static override get styles() {
    return getCss();
  }

  protected items_: DisclosureItem[];
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    this.items_ = [
      {
        icon: 'plant',
        text: this.i18n('disclosureAboutItem'),
      },
      {
        icon: 'google',
        text: this.i18n('disclosureTabItem'),
      },
      {
        icon: 'frame',
        text: this.i18n('disclosureDataItem'),
      },
      {
        icon: 'user',
        text: this.i18n(
            'disclosureAccountItem', loadTimeData.getString('userEmail')),
      },
    ];
  }

  override render() {
    chrome.metricsPrivate.recordUserAction(
        'Commerce.Compare.FirstRunExperience.Shown');
    return getHtml.bind(this)();
  }

  protected async acceptDisclosure_() {
    chrome.metricsPrivate.recordUserAction(
        'Commerce.Compare.FirstRunExperience.Accept');
    this.shoppingApi_.setProductSpecificationDisclosureAcceptVersion(
        ProductSpecificationsDisclosureVersion.kV1);

    // On accept, if `set_id` is available, open the existing set; otherwise
    // create a new set with `urls` and `name`.
    const args = JSON.parse(chrome.getVariableValue('dialogArguments'));
    const setId = args['set_id'];
    if (setId.length !== 0) {
      this.shoppingApi_.showProductSpecificationsSetForUuid(
          {value: setId}, false);
      chrome.send('dialogClose');
      return;
    }
    let name: string = args['name'];
    if (name.length === 0) {
      name = loadTimeData.getString('defaultTableTitle');
    }
    const urls: string[] = args['urls'];
    const inNewTab: boolean = args['in_new_tab'];
    const {createdSet} = await this.shoppingApi_.addProductSpecificationsSet(
        name, urls.map(url => ({url})));
    if (createdSet) {
      this.shoppingApi_.showProductSpecificationsSetForUuid(
          createdSet.uuid, inNewTab);
    }
    chrome.send('dialogClose');
  }

  protected declineDisclosure_() {
    chrome.metricsPrivate.recordUserAction(
        'Commerce.Compare.FirstRunExperience.Reject');
    this.shoppingApi_.declineProductSpecificationDisclosure();
    chrome.send('dialogClose');
  }

  protected onLearnMoreClicked_() {
    chrome.metricsPrivate.recordUserAction(
        'Commerce.Compare.FirstRunExperience.LearnMore');
    chrome.send('dialogClose');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-disclosure-app': DisclosureAppElement;
  }
}

customElements.define(DisclosureAppElement.is, DisclosureAppElement);
