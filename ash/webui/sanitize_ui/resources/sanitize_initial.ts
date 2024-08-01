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


const SanitizeInitialElementBase = I18nMixin(PolymerElement);

export class SanitizeInitialElement extends SanitizeInitialElementBase {
  static get is() {
    return 'sanitize-initial' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SanitizeInitialElement.is]: SanitizeInitialElement;
  }
}

customElements.define(SanitizeInitialElement.is, SanitizeInitialElement);
ColorChangeUpdater.forDocument().start();
