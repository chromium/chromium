// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-rivacy-guide-dialog' is a settings dialog that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import './privacy_guide_page.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

  private onClose_(event: Event) {
    // Closing the cr-dialog fires its own close event.
    event.stopPropagation();

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
