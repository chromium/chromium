// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/sidenav/sidenav.js';
import '../../settings_shared.css.js';
import './storybook_styles.css.js';
import './settings_dropdown_row_storybook.js';
import './settings_dropdown_v2_storybook.js';
import './settings_row_storybook.js';
import './settings_slider_row_storybook.js';
import './settings_slider_v2_storybook.js';
import './settings_toggle_v2_storybook.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../../assert_extras.js';

import {getTemplate} from './storybook_subpage.html.js';

interface SidenavItem {
  id: string;
  label: string;
}

export class SettingsStorybookSubpage extends PolymerElement {
  static get is() {
    return 'settings-storybook-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the selected page.
       */
      selectedIndex_: {
        type: Number,
        value: 0,
      },
    };
  }

  private selectedIndex_: number;

  override ready(): void {
    super.ready();

    // Hide left menu.
    const uiElement = castExists(document.body.querySelector('os-settings-ui'));
    uiElement.shadowRoot!.querySelector<HTMLElement>('#left')!.hidden = true;
  }

  private onSidenavSelect_(
      event: CustomEvent<{enabled: boolean, item: SidenavItem}>): void {
    if (event.detail.enabled) {
      this.selectedIndex_ = parseInt(event.detail.item.id, 10);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsStorybookSubpage.is]: SettingsStorybookSubpage;
  }
}

customElements.define(SettingsStorybookSubpage.is, SettingsStorybookSubpage);
