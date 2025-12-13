// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/427204855): Follow up with the actual implementation to
// visualize state of the TabStrip model.
console.info('TabStrip Internals WebUI loaded.');

import type {TabStripInternalsApiProxy} from './tab_strip_internals_api_proxy.js';
import {TabStripInternalsApiProxyImpl} from './tab_strip_internals_api_proxy.js';
import type {Container} from './tab_strip_internals.mojom-webui.js';

/**
 * Handles loading and rendering of TabStrip Internals state in the WebUI.
 */
export class TabStripInternalsApp {
  private proxy: TabStripInternalsApiProxy;
  private container: HTMLElement;

  constructor() {
    this.proxy = TabStripInternalsApiProxyImpl.getInstance();
    this.container = document.getElementById('tabstrip-container')!;
    this.initialize();
  }

  /**
   * Initializes the app by fetching initial data and subscribing to updates.
   */
  private initialize() {
    // Fetch the initial TabStrip data.
    this.proxy.getTabStripData()
        .then(({data}) => this.render(data))
        .catch(
            () => this.container.textContent = 'Failed to load TabStrip data.');

    // Subscribe to updates from the browser.
    this.proxy.getCallbackRouter().onTabStripUpdated.addListener(
        (data: Container) => {
          this.render(data);
        });
  }

  /**
   * Renders TabStrip data as formatted JSON.
   */
  private render(data: Container) {
    const tree = this.buildNode('Container', data);
    this.container.replaceChildren(tree);  // Trusted Types safe
  }

  /**
   * Recursively builds a DOM tree from an object with indentation based on
   * depth.
   */
  private buildNode(key: string, value: unknown, depth: number = 0):
      HTMLElement {
    const wrapper = document.createElement('div');
    wrapper.style.paddingLeft = `${depth * 4}px`;  // Indent based on depth

    if (value === null || value === undefined) {
      wrapper.textContent = `${key}: null`;
      return wrapper;
    }

    if (typeof value === 'object') {
      const details = document.createElement('details');
      details.open = true;

      const summary = document.createElement('summary');
      summary.textContent = key;
      summary.style.fontWeight = 'bold';
      details.appendChild(summary);

      if (Array.isArray(value)) {
        value.forEach((item, index) => {
          details.appendChild(this.buildNode(`[${index}]`, item, depth + 1));
        });
      } else {
        for (const [k, v] of Object.entries(value)) {
          details.appendChild(this.buildNode(k, v, depth + 1));
        }
      }

      wrapper.appendChild(details);
    } else {
      wrapper.textContent = `${key}: ${value}`;
    }

    return wrapper;
  }
}

// Bootstrap the application on page load.
document.addEventListener('DOMContentLoaded', () => {
  new TabStripInternalsApp();
});
