// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'sanitize-initial' is a dialog shown to kickstart resetting settings to a
 * safe default (aka sanitize).
 */
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cros_components/button/button.js';
import './sanitize_shared.css.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sanitize_initial.html.js';
import {SettingsResetter, SettingsResetterInterface} from './sanitize_ui.mojom-webui.js';

// Implemented by Ash, provides the interface that kickstarts the sanitize
// process.
let resetterInstance: SettingsResetterInterface|null = null;

const SanitizeInitialElementBase = I18nMixin(PolymerElement);

function getResetter(): SettingsResetterInterface {
  if (!resetterInstance) {
    resetterInstance = SettingsResetter.getRemote();
  }
  return resetterInstance;
}

export class SanitizeInitialElement extends SanitizeInitialElementBase {
  static get is() {
    return 'sanitize-initial' as const;
  }

  static get template() {
    return getTemplate();
  }

  private onCancel(): void {
    window.close();
  }

  private onPerformSanitize(): void {
    getResetter().performSanitizeSettings();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SanitizeInitialElement.is]: SanitizeInitialElement;
  }
}

customElements.define(SanitizeInitialElement.is, SanitizeInitialElement);
ColorChangeUpdater.forDocument().start();
