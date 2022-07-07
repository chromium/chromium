// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './folder_selector.js';

import {getTemplate} from './manage_mirrorsync.html.js';

/**
 * ManageMirrorSync represents the top level web component that tracks the
 * state for the chrome://manage-mirrorsync dialog.
 */
class ManageMirrorSync extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode: 'open'})
        .appendChild(getTemplate().content.cloneNode(true));
  }

  /**
   * Invoked when the <manage-mirrorsync> web component is attached to the DOM.
   */
  connectedCallback() {
    this.shadowRoot?.querySelector('#selected')
        ?.addEventListener('click', this.onSpecifyFolderSelection.bind(this));
  }

  /**
   * Shows the folder-selector web component if the specific folder selection
   * option was chosen. Doesn't hide after initially shown.
   */
  private onSpecifyFolderSelection(event: Event) {
    if (!event.currentTarget) {
      return;
    }
    const isChecked = (event.currentTarget as HTMLInputElement).checked;
    if (!isChecked) {
      return;
    }
    this.shadowRoot?.querySelector('folder-selector')
        ?.toggleAttribute('hidden', false);
  }
}

customElements.define('manage-mirrorsync', ManageMirrorSync);
