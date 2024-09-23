// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OOBE Modal Dialog
 *
 * Implements the 'OOBE Modal Dialog' according to MD specs.
 *
 * The dialog provides two properties that can be set directly from HTML.
 *  - titleKey - ID of the localized string to be used for the title.
 *  - contentKey - ID of the localized string to be used for the content.
 *
 *  Alternatively, one can set their own title and content into the 'title'
 *  and 'content' slots.
 *
 *  Buttons are optional and go into the 'buttons' slot. If none are specified,
 *  a default button with the text 'Close' will be shown. Users might want to
 *  trigger some action on their side by using 'on-close=myMethod'.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../buttons/oobe_text_button.js';
import '../common_styles/oobe_common_styles.css.js';

import {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeFocusMixin} from '../mixins/oobe_focus_mixin.js';
import {OobeI18nMixin} from '../mixins/oobe_i18n_mixin.js';

import {getTemplate} from './oobe_modal_dialog.html.js';

const OobeModalDialogBase = OobeI18nMixin(OobeFocusMixin(PolymerElement));

export class OobeModalDialog extends OobeModalDialogBase {
  static get is() {
    return 'oobe-modal-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /* The ID of the localized string to be used as title text when no "title"
       * slot elements are specified.
       */
      titleKey: {
        type: String,
      },
      /* The ID of the localized string to be used as the content when no
       * "content" slot elements are specified.
       */
      contentKey: {
        type: String,
      },

      /**
       * True if close button should be hidden.
       */
      shouldHideCloseButton: {
        type: Boolean,
        value: false,
      },

      /**
       * True if title row should be hidden.
       */
      shouldHideTitleRow: {
        type: Boolean,
        value: false,
      },

      /**
       * True if confirmation dialog backdrop should be hidden.
       */
      shouldHideBackdrop: {
        type: Boolean,
        value: false,
      },
    };
  }

  private titleKey: string;
  private contentKey: string;
  private shouldHideCloseButton: boolean;
  private shouldHideTitleRow: boolean;
  private shouldHideBackdrop: boolean;

  private getModalDialog(): CrDialogElement {
    const modalDialog = this.shadowRoot?.querySelector('#modalDialog');
    assert(modalDialog instanceof CrDialogElement);
    return modalDialog;
  }

  get open(): boolean {
    return this.getModalDialog().open;
  }

  override ready(): void {
    super.ready();
  }

  /*
   * Shows the modal dialog and changes the focus to the first focusable
   * element.
   */
  showDialog(): void {
    chrome.send('enableShelfButtons', [false]);
    this.getModalDialog().showModal();
    this.focusMarkedElement(this);
  }

  hideDialog(): void {
    this.getModalDialog().close();
  }

  private onClose(): void {
    chrome.send('enableShelfButtons', [true]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeModalDialog.is]: OobeModalDialog;
  }
}

customElements.define(OobeModalDialog.is, OobeModalDialog);
