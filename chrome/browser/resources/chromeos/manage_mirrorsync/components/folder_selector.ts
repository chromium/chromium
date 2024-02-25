// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './folder_selector.html.js';

/**
 * Retrieves the parent folder path of the supplied `folderPath`. Useful to
 * identify the list container to place the folder element.
 */
function getParentPath(folderPath: string): string {
  const folderParts = folderPath.split('/');
  const parentPath = folderParts.slice(0, folderParts.length - 1).join('/');
  if (parentPath.length === 0) {
    return '/';
  }
  return parentPath;
}

/**
 * Retrieves the folder name as the final element of an absolute path.
 */
function getFolderName(folderPath: string): string {
  const folderParts = folderPath.split('/');
  return folderParts[folderParts.length - 1]!;
}

/**
 * Helper function to convert a supplied folder path to it's querySelector
 * variant using the data-full-path key and escaping double quotes.
 */
function selectorFromPath(folderPath: string): string {
  return `input[data-full-path="${folderPath.replace(/"/g, '\\"')}"]`;
}

/**
 * FolderSelector presents a folder hierarchy of checkboxes representing the
 * underlying folder structure. The items are lazily loaded as required.
 */
export class FolderSelector extends HTMLElement {
  /* The <template> fragment used to create new elements. */
  private folderSelectorTemplate: HTMLTemplateElement;

  /* A Set of currently selected folders. */
  private selectedFolders: Set<string> = new Set();

  constructor() {
    super();
    this.attachShadow({mode: 'open'})
        .appendChild(getTemplate().content.cloneNode(true));

    this.folderSelectorTemplate =
        this.shadowRoot!.getElementById('folder-selector-template') as
        HTMLTemplateElement;
  }

  /**
   * Once the <folder-selector> component has been connected to the DOM, this
   * lifecycle callback is invoked.
   */
  connectedCallback() {
    // Register event listeners on the root list element.
    const li = this.shadowRoot?.querySelector('#select-folders > ul > li') as
        HTMLLIElement;
    li.addEventListener('click', (event) => this.onPathExpanded(event, '/'));

    const selector = selectorFromPath('/');
    const input = this.shadowRoot?.querySelector(selector) as HTMLInputElement;
    input.addEventListener('click', event => {
      // The <li> enclosing the <input> also has an event listener so don't
      // propagate once the click has been received.
      event.stopPropagation();
      this.onPathSelected(event, '/');
    });
  }

  /**
   * Add an array of paths to the DOM. These paths must all share the same
   * parent.
   */
  async addChildFolders(folderPaths: string[]) {
    const parentElements: Map<string, HTMLInputElement> = new Map();
    let parentSelected: boolean = false;
    /**
     * Get the parent container and in the process cache the parent element. All
     * folders coming in via addChildFolders share the same parent.
     * @param folderPath
     */
    const getParentContainer = (folderPath: string) => {
      const parentPath = getParentPath(folderPath);
      if (!parentElements.has(parentPath)) {
        const parentSelector = selectorFromPath(parentPath);
        const parentElement =
            this.shadowRoot!.querySelector<HTMLInputElement>(parentSelector)!;
        parentElements.set(parentPath, parentElement);
        parentSelected = parentElement.checked;
      }
      parentSelected = parentElements.get(parentPath)!.checked;
      return parentElements.get(parentPath)!.parentElement!.nextElementSibling!;
    };

    for (const path of folderPaths) {
      if (this.shadowRoot?.querySelector(selectorFromPath(path))) {
        continue;
      }
      const ulContainer = getParentContainer(path);
      const newElement = this.createNewFolderSelection(path, parentSelected);
      ulContainer?.appendChild(newElement);
    }
  }

  /**
   * Returns the list of paths that are currently selected.
   */
  get selectedPaths() {
    return Array.from(this.selectedFolders.values());
  }

  /**
   * Event listener for when a checkbox for a path is selected. We want to
   * update all descendants to be disabled and checked and ensure the selected
   * path is being kept track of.
   */
  private onPathSelected(event: Event, path: string) {
    const input = (event.currentTarget as HTMLInputElement);
    const {checked} = input;
    if (checked) {
      this.selectedFolders.add(path);
    } else {
      this.selectedFolders.delete(path);
    }
    const children =
        input.parentElement!.nextElementSibling!.querySelectorAll('input');
    for (const child of children) {
      child.toggleAttribute('disabled', checked);
      child.toggleAttribute('checked', checked);
    }
  }

  /**
   * Event listener for when a path has been clicked (excluding the checkbox).
   * Dispatches an event to enable the <manage-mirrorsync> to fetch the children
   * folders.
   */
  private onPathExpanded(event: Event, path: string) {
    const li = (event.currentTarget as HTMLElement) as HTMLLIElement;
    li.toggleAttribute('expanded');
    if (li.hasAttribute('retrieved')) {
      return;
    }
    this.dispatchEvent(new CustomEvent(
        FOLDER_EXPANDED, {bubbles: true, composed: true, detail: path}));
    li.toggleAttribute('retrieved', true);
  }

  /**
   * Creates a new folder selection and assigns the requisite event listeners.
   * Uses the shadowRoot <template> fragment that contains a minimal
   * representation and builds on top of that.
   */
  private createNewFolderSelection(folderPath: string, selected: boolean):
      HTMLElement {
    const newFolderTemplate =
        this.folderSelectorTemplate.content.cloneNode(true) as HTMLElement;
    const textNode = document.createTextNode(getFolderName(folderPath));

    const li = newFolderTemplate.querySelector('li')!;
    li.appendChild(textNode);
    li.addEventListener(
        'click', (event) => this.onPathExpanded(event, folderPath));

    const input = newFolderTemplate.querySelector('input[name="folders"]')!;
    input.setAttribute('data-full-path', folderPath);
    input.toggleAttribute('disabled', selected);
    input.toggleAttribute('checked', selected);

    // TODO(b/237066325): Add one event listener to the <folder-selector> and
    // switch on the element clicked to identify whether it is expanded or
    // selected to avoid too many event listeners.
    input.addEventListener('click', event => {
      event.stopPropagation();
      this.onPathSelected(event, folderPath);
    });

    return newFolderTemplate;
  }
}

/**
 * The available events that occur from this web component.
 */
export const FOLDER_EXPANDED = 'folder_selected';

/**
 * Custom event alias, the event.detail key has the folder path that indicates
 * the folder to expand.
 */
export type FolderExpandedEvent = CustomEvent<string>;

declare global {
  interface HTMLElementEventMap {
    [FOLDER_EXPANDED]: FolderExpandedEvent;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'folder-selector': FolderSelector;
  }
}

customElements.define('folder-selector', FolderSelector);
