// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class OrganizerPanelAppElement extends CrLitElement {
  static get is() {
    return 'organizer-panel-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shortcut_: {type: String},
    };
  }

  protected accessor shortcut_: string = loadTimeData.getString('shortcutText');
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-panel-app': OrganizerPanelAppElement;
  }
}

customElements.define(OrganizerPanelAppElement.is, OrganizerPanelAppElement);
