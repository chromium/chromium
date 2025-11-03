// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {ComposeboxMode} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './action_chips.css.js';
import {getHtml} from './action_chips.html.js';

/**
 * TODO: Move the enum to the Mojo model once it's created.
 * Elements on the Action Chips. This enum must match the numbering for
 * ActionChipsType in enums.xml. These values are persisted to logs. Entries
 * should not be renumbered, removed or reused.
 */

// LINT.IfChange(ActionChipsType)
export enum ActionChipsType {
  CREATE_IMAGE = 0,
  DEEP_SEARCH = 1,
  MAX_VALUE = DEEP_SEARCH,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:ActionChipsType)

// Records a click metric for the given action chip type.
function recordClick(element: ActionChipsType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Click', element, ActionChipsType.MAX_VALUE + 1);
}

// Records an impression metric for the given action chip type.
function recordShown(element: ActionChipsType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Shown', element, ActionChipsType.MAX_VALUE + 1);
}

/**
 * The element for displaying Action Chips.
 */
export class ActionChipsElement extends CrLitElement {
  static get is() {
    return 'ntp-action-chips';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // TODO (crbug.com/453650248): Move the impression metrics logging to the
  // handler logic once the changes are merged in. Right now we are only
  // statically showing these two chips.
  override firstUpdated() {
    recordShown(ActionChipsType.CREATE_IMAGE);
    recordShown(ActionChipsType.DEEP_SEARCH);
  }


  protected onCreateImageClick_() {
    recordClick(ActionChipsType.CREATE_IMAGE);
    this.onActionChipClick_('Create an image ', ComposeboxMode.CREATE_IMAGE);
  }

  protected onDeepSearchClick_() {
    recordClick(ActionChipsType.DEEP_SEARCH);
    this.onActionChipClick_('Help me research ', ComposeboxMode.DEEP_SEARCH);
  }

  private onActionChipClick_(query: string, mode: ComposeboxMode) {
    this.fire(
        'action-chip-click', {searchboxText: query, contextFiles: [], mode});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-action-chips': ActionChipsElement;
  }
}

customElements.define(ActionChipsElement.is, ActionChipsElement);
