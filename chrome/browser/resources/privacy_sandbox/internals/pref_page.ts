// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './pref_display.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './pref_page.html.js';

export interface PrivacySandboxInternalsPrefGroup {
  id: string;
  title: string;
  prefPrefixes: string[];
}

export interface PrivacySandboxInternalsPrefPageConfig {
  id: string;
  title: string;
  prefGroups: PrivacySandboxInternalsPrefGroup[];
}

export class PrefPage extends CustomElement {
  pageConfig: PrivacySandboxInternalsPrefPageConfig|null = null;
  isDefault: boolean = false;

  static get is() {
    return 'pref-page';
  }

  connectedCallback() {
    const parent = this.parentElement;
    if (!parent) {
      console.error('<pref-page> has no parent or has a non-element parent.');
      return;
    }

    if (!this.pageConfig) {
      console.error('A pageConfig must be set to create a <pref-page>.');
      return;
    }

    const pageTemplateEl = document.createElement('template');
    pageTemplateEl.innerHTML = getTemplate();

    // Customize the tab slot for the left sidebar
    const fragment = pageTemplateEl.content.cloneNode(true) as DocumentFragment;
    const pageTab = fragment.querySelector<HTMLElement>('[slot="tab"]')!;
    pageTab.textContent = this.pageConfig.title;
    pageTab.dataset['pageName'] = this.pageConfig.id;
    pageTab.id = this.pageConfig.id + '-prefs-tab';

    // Add the 'selected' attribute for the default page.
    if (this.isDefault) {
      pageTab.toggleAttribute('selected', true);
    }
    const pagePanel = fragment.querySelector<HTMLElement>('[slot="panel"]')!;
    this.pageConfig.prefGroups.forEach((prefGroup) => {
      const template =
          fragment.querySelector<HTMLTemplateElement>('#pref-group-template')!;
      const clone = template.content.cloneNode(true) as DocumentFragment;

      clone.querySelector('.pref-group-title')!.textContent = prefGroup.title;
      clone.querySelector<HTMLElement>('.pref-group-panel')!.id =
          prefGroup.id + '-prefs-panel';
      pagePanel.appendChild(clone);
    });

    // Move the nodes from the fragment to be direct children of the parent.
    // This makes them available for slotting by <cr-frame-list>.
    parent.appendChild(fragment);

    // Remove the <pref-page> node from the DOM after its children have been
    // moved into <cr-frame-list>.
    this.remove();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pref-page': PrefPage;
  }
}

customElements.define('pref-page', PrefPage);
