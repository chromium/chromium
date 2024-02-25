// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './folder_selector.js';

import {BrowserProxy} from '../browser_proxy.js';

import {FOLDER_EXPANDED, FolderExpandedEvent, FolderSelector} from './folder_selector.js';
import {getTemplate} from './manage_mirrorsync.html.js';

/**
 * ManageMirrorSync represents the top level web component that tracks the
 * state for the chrome://manage-mirrorsync dialog.
 */
class ManageMirrorSync extends HTMLElement {
  /**
   * The <folder-selector> component on the page. Gets set when the
   * <manage-mirrorsync> components connects to the DOM.
   */
  private folderSelector: FolderSelector|null = null;

  /**
   * Bind the onSpecifyFolderSelection event listener to enable removal of it
   * once the first click has been registered.
   */
  private onSpecifyFolderSelectionBound: (event: Event) => void;

  constructor() {
    super();
    this.attachShadow({mode: 'open'})
        .appendChild(getTemplate().content.cloneNode(true));

    this.onSpecifyFolderSelectionBound =
        this.onSpecifyFolderSelection.bind(this);
  }

  /**
   * Invoked when the <manage-mirrorsync> web component is attached to the DOM.
   */
  connectedCallback() {
    this.shadowRoot!.getElementById('selected')!.addEventListener(
        'click', this.onSpecifyFolderSelectionBound);
    this.folderSelector = this.shadowRoot!.querySelector('folder-selector');
  }

  /**
   * Shows the folder-selector web component if the specific folder selection
   * option was chosen. Doesn't hide after initially shown.
   */
  private async onSpecifyFolderSelection(event: Event) {
    if (!event.currentTarget) {
      return;
    }
    const isChecked = (event.currentTarget as HTMLInputElement).checked;
    if (!isChecked) {
      return;
    }
    this.folderSelector?.addEventListener(
        FOLDER_EXPANDED,
        (event: FolderExpandedEvent) => this.onFolderExpanded(event.detail));
    this.folderSelector?.toggleAttribute('hidden', false);
    this.shadowRoot!.getElementById('selected')!.removeEventListener(
        'click', this.onSpecifyFolderSelectionBound);

    // Kick off retrieving the child folders for the root path.
    this.onFolderExpanded('/');
  }

  /**
   * When a folder gets expanded, kick off the retrieval of the child folders.
   * This will call back into the <folder-selector> element with the data on
   * retrieval.
   */
  private async onFolderExpanded(path: string) {
    const proxy = BrowserProxy.getInstance().handler;
    const childFolders = await proxy.getChildFolders({path});
    const folderPaths = childFolders.paths.map(({path}) => path);
    this.folderSelector?.addChildFolders(folderPaths);
  }
}

customElements.define('manage-mirrorsync', ManageMirrorSync);
