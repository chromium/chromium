// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-main' displays the selected settings page.
 */
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/js/search_highlight_utils.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './managed_footnote.js';
import '../main_page_container/main_page_container.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PrefsState} from '../common/types.js';
import type {OsPageAvailability} from '../os_page_availability.js';

import {getTemplate} from './os_settings_main.html.js';

declare global {
  interface HTMLElementEventMap {
    'showing-main-page': CustomEvent;
    'showing-subpage': CustomEvent;
  }
}

export class OsSettingsMainElement extends PolymerElement {
  static get is() {
    return 'os-settings-main' as const;
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

      isShowingSubpage_: Boolean,

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Dictionary defining page availability.
       */
      pageAvailability: Object,
    };
  }

  prefs: PrefsState;
  toolbarSpinnerActive: boolean;
  pageAvailability: OsPageAvailability;
  private isShowingSubpage_: boolean;

  override ready(): void {
    super.ready();

    this.addEventListener('showing-main-page', this.onShowingMainPage_);
    this.addEventListener('showing-subpage', this.onShowingSubpage_);
  }

  private onShowingMainPage_(): void {
    this.isShowingSubpage_ = false;
  }

  private onShowingSubpage_(): void {
    this.isShowingSubpage_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsMainElement.is]: OsSettingsMainElement;
  }
}

customElements.define(OsSettingsMainElement.is, OsSettingsMainElement);
