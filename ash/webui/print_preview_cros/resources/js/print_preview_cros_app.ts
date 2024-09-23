// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './destination_select.js';
import './summary_panel.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './print_preview_cros_app.html.js';
import {PrintPreviewCrosAppController} from './print_preview_cros_app_controller.js';

/**
 * @fileoverview
 * 'print-preview-cros-app' is the main landing page for the print preview
 * for ChromeOS app.
 */

export class PrintPreviewCrosAppElement extends PolymerElement {
  static get is() {
    return 'print-preview-cros-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  private controller = new PrintPreviewCrosAppController();

  override ready(): void {
    super.ready();
    ColorChangeUpdater.forDocument().start();
  }

  getControllerForTesting(): PrintPreviewCrosAppController {
    return this.controller;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrintPreviewCrosAppElement.is]: PrintPreviewCrosAppElement;
  }
}

customElements.define(
    PrintPreviewCrosAppElement.is, PrintPreviewCrosAppElement);
