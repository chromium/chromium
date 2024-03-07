// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';
import './css/about_sys.css.js';
import './css/logs_map_page.css.js';
import './feedback_shared_styles.css.js';
// <if expr="chromeos_ash">
import './js/jelly_colors.js';

// </if>
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './autofill_metadata_app.html.js';
import {FeedbackBrowserProxyImpl} from './js/feedback_browser_proxy.js';
import {createLogsMapTable} from './js/logs_map_page.js';

export class AutofillMetadataAppElement extends PolymerElement {
  static get is() {
    return 'autofill-metadata-app';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();
    const dialogArgs =
        FeedbackBrowserProxyImpl.getInstance().getDialogArguments();
    if (!dialogArgs) {
      return;
    }

    this.createAutofillMetadataTable(dialogArgs);
  }

  /**
   * Builds the autofill metadata table. Constructs the map entries for the logs
   * page by parsing the input json to readable string.
   */
  private createAutofillMetadataTable(dialogArgs: string) {
    const autofillMetadata = JSON.parse(dialogArgs);

    const items: chrome.feedbackPrivate.LogsMapEntry[] = [];

    if (autofillMetadata.triggerFormSignature) {
      items.push({
        key: 'trigger_form_signature',
        value: autofillMetadata.triggerFormSignature,
      });
    }

    if (autofillMetadata.triggerFieldSignature) {
      items.push({
        key: 'trigger_field_signature',
        value: autofillMetadata.triggerFieldSignature,
      });
    }

    if (autofillMetadata.lastAutofillEvent) {
      items.push({
        key: 'last_autofill_event',
        value: JSON.stringify(autofillMetadata.lastAutofillEvent, null, 1),
      });
    }

    if (autofillMetadata.formStructures) {
      items.push({
        key: 'form_structures',
        value: JSON.stringify(autofillMetadata.formStructures, null, 1),
      });
    }

    createLogsMapTable(items, this.shadowRoot!);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'autofill-metadata-app': AutofillMetadataAppElement;
  }
}

customElements.define(
    AutofillMetadataAppElement.is, AutofillMetadataAppElement);
