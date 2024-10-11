// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_compare_subpage.html.js';

export class SettingsAiCompareSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-ai-compare-subpage';
  }

  static get template() {
    return getTemplate();
  }

  private onCompareLinkoutClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('compareDataHomeUrl'));
  }

  private onLearnMoreClick_(event: Event) {
    // Stop the propagation of events, so that clicking on the 'Learn More' link
    // won't trigger the external linkout action on the parent cr-link-row
    // element.
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-compare-subpage': SettingsAiCompareSubpageElement;
  }
}

customElements.define(
    SettingsAiCompareSubpageElement.is, SettingsAiCompareSubpageElement);
