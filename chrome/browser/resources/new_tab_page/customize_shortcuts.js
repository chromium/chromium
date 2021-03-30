// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mini_page.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NewTabPageProxy} from './new_tab_page_proxy.js';

/** Element that lets the user configure shortcut settings. */
class CustomizeShortcutsElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-shortcuts';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      customLinksEnabled_: Boolean,

      /** @private */
      hide_: Boolean,
    };
  }

  constructor() {
    super();
    const {callbackRouter, handler} = NewTabPageProxy.getInstance();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = handler;
    /** @private {?number} */
    this.setMostVisitedInfoListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setMostVisitedInfoListenerId_ =
        this.callbackRouter_.setMostVisitedInfo.addListener(info => {
          this.customLinksEnabled_ = info.customLinksEnabled;
          this.hide_ = !info.visible;
        });
    this.pageHandler_.updateMostVisitedInfo();
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.setMostVisitedInfoListenerId_));
  }

  apply() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_, /* visible= */ !this.hide_);
  }

  /**
   * @return {string}
   * @private
   */
  getCustomLinksAriaPressed_() {
    return !this.hide_ && this.customLinksEnabled_ ? 'true' : 'false';
  }

  /**
   * @return {string}
   * @private
   */
  getCustomLinksSelected_() {
    return !this.hide_ && this.customLinksEnabled_ ? 'selected' : '';
  }

  /**
   * @return {string}
   * @private
   */
  getHideClass_() {
    return this.hide_ ? 'selected' : '';
  }

  /**
   * @return {string}
   * @private
   */
  getMostVisitedAriaPressed_() {
    return !this.hide_ && !this.customLinksEnabled_ ? 'true' : 'false';
  }

  /**
   * @return {string}
   * @private
   */
  getMostVisitedSelected_() {
    return !this.hide_ && !this.customLinksEnabled_ ? 'selected' : '';
  }

  /** @private */
  onCustomLinksClick_() {
    if (!this.customLinksEnabled_) {
      this.pageHandler_.onCustomizeDialogAction(
          newTabPage.mojom.CustomizeDialogAction.kShortcutsCustomLinksClicked);
    }
    this.customLinksEnabled_ = true;
    this.hide_ = false;
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onHideChange_(e) {
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction
            .kShortcutsVisibilityToggleClicked);
    this.hide_ = e.detail;
  }


  /** @private */
  onMostVisitedClick_() {
    if (this.customLinksEnabled_) {
      this.pageHandler_.onCustomizeDialogAction(
          newTabPage.mojom.CustomizeDialogAction.kShortcutsMostVisitedClicked);
    }
    this.customLinksEnabled_ = false;
    this.hide_ = false;
  }
}

customElements.define(CustomizeShortcutsElement.is, CustomizeShortcutsElement);
