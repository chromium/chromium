// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-rivacy-guide-dialog' is a settings dialog that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import './privacy_guide_page.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_guide_dialog.html.js';

export interface SettingsPrivacyGuideDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsPrivacyGuideDialogElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-guide-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    // TODO(crbug/1215630): Instead of this focus code, it should be possible to
    // use |autofocus| on the corresponding element in the cr-dialog to put the
    // focus on it when the dialog is shown. For an unknown reason this does not
    // work atm [1]. Use |autofocus| once this reason has been found and fixed.
    // [1] https://crrev.com/c/3541986/comments/a3a6bdfb_3e1e0e29
    const elementToFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#backToSettingsButton')!;
    afterNextRender(this, () => elementToFocus!.focus());
  }

  private onClose_(event: Event) {
    // Closing the cr-dialog fires its own close event.
    event.stopPropagation();

    this.$.dialog.close();
  }

  private onSettingsBackClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-guide-dialog': SettingsPrivacyGuideDialogElement;
  }
}

customElements.define(
    SettingsPrivacyGuideDialogElement.is, SettingsPrivacyGuideDialogElement);
