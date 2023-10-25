// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {AutocompleteControllerType, AutocompleteMatch} from '../omnibox.mojom-webui.js';
import {clearChildren, createEl, setFormattedClipboardForMl, signalNames} from '../omnibox_util.js';

import {MlBrowserProxy, ResponseFilter} from './ml_browser_proxy.js';
// @ts-ignore:next-line
import sheet from './ml_table.css' assert {type : 'css'};
import {getTemplate} from './ml_table.html.js';

export class MlTableElement extends CustomElement {
  private mlBrowserProxy_: MlBrowserProxy;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    signalNames.forEach(signalName => {
      createEl(
          'div', this.getRequiredElement('.thead .tr'), ['th'],
          signalName.replaceAll(/[A-Z]/g, ' $&').toLowerCase());
    });

    this.$all<HTMLDivElement>('.thead .th')
        .forEach(th => th.title = th.textContent!);

    Object.values(ResponseFilter).forEach(responseFilter => {
      createEl(
          'option', this.getRequiredElement('#table-filter'), [],
          responseFilter)
          .value = responseFilter;
    });

    this.getRequiredElement('#table-filter').addEventListener('input', () => {
      this.$all('.tbody').forEach(clearChildren);
    });
  }

  set mlBrowserProxy(mlBrowserProxy: MlBrowserProxy) {
    this.mlBrowserProxy_ = mlBrowserProxy;
    mlBrowserProxy.addResponseListener(
        (...args) => this.onNewResponse(...args));
  }

  onNewResponse(
      responseFilter: ResponseFilter,
      controllerType: AutocompleteControllerType, input: string,
      matches: AutocompleteMatch[]) {
    if (!matches.length) {
      return;
    }
    if (this.getRequiredElement<HTMLSelectElement>('#table-filter').value ===
            responseFilter ||
        controllerType === AutocompleteControllerType.kMlDisabledDebug) {
      this.setTable(controllerType, input, matches);
    }
  }

  private setTable(
      controllerType: AutocompleteControllerType, inputText: string,
      result: AutocompleteMatch[]) {
    if (controllerType === AutocompleteControllerType.kDebug) {
      return;
    }

    const tbody =
        controllerType === AutocompleteControllerType.kMlDisabledDebug ?
        this.getRequiredElement('#traditional-response') :
        this.getRequiredElement('#ml-response');
    clearChildren(tbody);

    const headers = this.getRequiredElement('.thead .tr').children;

    result.forEach(match => {
      const additionalInfo = Object.fromEntries(
          match.additionalInfo.map(tuple => Object.values(tuple)));

      const matchDetails = [
        inputText,
        match.providerName,
        match.contents,
        match.description,
        match.relevance,
        additionalInfo['ml model output'] || '',
        additionalInfo['ml legacy relevance'] || '',
      ];
      const signalValues = Object.values(match.scoringSignals);

      const tr = createEl('div', tbody, ['tr']);
      tr.addEventListener('click', async () => {
        this.$all('.tbody .tr')
            .forEach(tr2 => tr2.classList.toggle('selected', tr2 === tr));
        this.dispatchEvent(
            new CustomEvent('match-selected', {detail: match.scoringSignals}));
        const promise = setFormattedClipboardForMl(
            Object.fromEntries(matchDetails.map(
                (value, i) => [headers[i]!.textContent, value])),
            match.scoringSignals, '', await this.mlBrowserProxy_.modelVersion);
        this.dispatchEvent(new CustomEvent('copied', {detail: promise}));
      });

      [...matchDetails, ...signalValues].forEach(
          value => createEl('div', tr, ['td'], value));

      assert(
          tr.childElementCount ===
          this.getRequiredElement('.thead .tr').childElementCount);
    });
  }
}

customElements.define('ml-table', MlTableElement);
