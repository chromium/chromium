// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '../common_styles/oobe_common_styles.m.js';
import '../common_styles/oobe_dialog_host_styles.m.js';
import '../oobe_vars/oobe_shared_vars_css.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeFocusBehavior, OobeFocusBehaviorInterface} from '../behaviors/oobe_focus_behavior.js';
import {OobeScrollableBehavior, OobeScrollableBehaviorInterface} from '../behaviors/oobe_scrollable_behavior.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeScrollableBehaviorInterface}
 * @implements {OobeFocusBehaviorInterface}
 */
const OobeContentDialogBase =
    mixinBehaviors([OobeFocusBehavior, OobeScrollableBehavior], PolymerElement);


/** @polymer */
export class OobeContentDialog extends OobeContentDialogBase {
  static get template() {
    return html`{__html_template__}`;
  }

  static get is() {
    return 'oobe-content-dialog';
  }

  static get properties() {
    return {
      /**
       * Supports dialog which is shown without buttons.
       */
      noButtons: {
        type: Boolean,
        value: false,
      },

      /**
       * If set, prevents lazy instantiation of the dialog.
       */
      noLazy: {
        type: Boolean,
        value: false,
        observer: 'onNoLazyChanged_',
      },
    };
  }

  onBeforeShow() {
    this.shadowRoot.querySelector('#lazy').get();
    var contentContainer = this.shadowRoot.querySelector('#contentContainer');
    var scrollContainer = this.shadowRoot.querySelector('#scrollContainer');
    if (!scrollContainer || !contentContainer) {
      return;
    }
    this.initScrollableObservers(scrollContainer, contentContainer);
  }

  focus() {
    /**
     * TODO (crbug.com/1159721): Fix this once event flow of showing step in
     * display_manager is updated.
     */
    this.show();
  }

  show() {
    this.focusMarkedElement(this);
  }

  /** @private */
  onNoLazyChanged_() {
    if (this.noLazy) {
      this.shadowRoot.querySelector('#lazy').get();
    }
  }
}

customElements.define(OobeContentDialog.is, OobeContentDialog);