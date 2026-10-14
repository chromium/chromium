// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {ExplainFulfillment, ExplainFulfillmentRemote} from './explain_fulfillment.mojom-webui.js';
import type {ExplainFulfillmentInterface} from './explain_fulfillment.mojom-webui.js';
import {renderMarkdown} from './markdown.js';

// The card for the C++'s `ExplainSuggestion`. Shows the explanation, and has
// the browser open Gemini.
export class ExplainFulfillmentElement extends HTMLElement {
  static get is() {
    return 'glic-explain-fulfillment';
  }

  // Set by create(). Tests may replace it.
  fulfillment: ExplainFulfillmentInterface|null = null;

  connectedCallback() {
    const explanation = document.createElement('div');
    explanation.className = 'explanation';
    // Links search for their text.
    explanation.addEventListener('click', e => {
      const link = (e.target as Element).closest('a');
      if (link) {
        e.preventDefault();
        this.fulfillment?.openTabForSearch(link.textContent);
      }
    });
    const askGemini = document.createElement('cr-button');
    askGemini.className = 'ask-gemini';
    const askGeminiIcon = document.createElement('div');
    askGeminiIcon.className = 'ask-gemini-icon';
    askGeminiIcon.slot = 'prefix-icon';
    askGemini.append(askGeminiIcon, loadTimeData.getString('askGeminiMore'));
    askGemini.addEventListener('click', () => this.fulfillment?.askGemini());
    this.replaceChildren(explanation, askGemini);
    this.fulfillment?.getExplanation().then(response => {
      explanation.replaceChildren(renderMarkdown(response.explanation));
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-explain-fulfillment': ExplainFulfillmentElement;
  }
}

customElements.define(ExplainFulfillmentElement.is, ExplainFulfillmentElement);

export function create() {
  const remote = new ExplainFulfillmentRemote();
  const element = document.createElement('glic-explain-fulfillment');
  element.fulfillment = remote;
  return {element, interfaceName: ExplainFulfillment.$interfaceName, remote};
}
