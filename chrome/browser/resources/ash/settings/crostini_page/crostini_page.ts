// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-page' is the settings page for enabling Crostini.
 * Crostini Containers run Linux inside a Termina VM, allowing
 * the user to run Linux apps on their Chromebook.
 */

import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../settings_shared.css.js';
import '../guest_os/guest_os_shared_paths.js';
import './crostini_arc_adb.js';
import './crostini_export_import.js';
import './crostini_extra_containers.js';
import './crostini_port_forwarding.js';
import './crostini_shared_usb_devices.js';
import './crostini_subpage.js';
import './bruschetta_subpage.js';
import './crostini_settings_card.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './crostini_page.html.js';

export class SettingsCrostiniPageElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },
      section_: {
        type: Number,
        value: Section.kCrostini,
        readOnly: true,
      },
    };
  }

  prefs: PrefsState;
  private section_: Section;
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCrostiniPageElement.is]: SettingsCrostiniPageElement;
  }
}

customElements.define(
    SettingsCrostiniPageElement.is, SettingsCrostiniPageElement);
