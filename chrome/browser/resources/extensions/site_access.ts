// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';
import './shared_style.js';
import './shared_vars.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';

interface ExtensionsSiteAccessElement {
  $: {
    closeButton: HTMLElement,
  };
}

const ExtensionsSiteAccessElementBase =
    I18nMixin(CrContainerShadowMixin(PolymerElement)) as
    {new (): PolymerElement & I18nMixinInterface};

class ExtensionsSiteAccessElement extends ExtensionsSiteAccessElementBase {
  static get is() {
    return 'extensions-site-access';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The underlying ExtensionInfo for the extension site access items being
       * displayed.
       */
      extensionInfo: Object,
    };
  }

  extensionInfo: chrome.developerPrivate.ExtensionInfo;

  ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
  }

  /**
   * Focuses the back button when page is loaded.
   */
  private onViewEnterStart_() {
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
  }

  private onCloseButtonClick_() {
    navigation.navigateTo(
        {page: Page.DETAILS, extensionId: this.extensionInfo.id});
  }
}

customElements.define(
    ExtensionsSiteAccessElement.is, ExtensionsSiteAccessElement);
