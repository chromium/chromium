// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getTemplate} from './app.html.js';
import type {SiteEngagementDetails, SiteEngagementDetailsProviderInterface} from './site_engagement_details.mojom-webui.js';
import {SiteEngagementDetailsProvider} from './site_engagement_details.mojom-webui.js';

/**
 * Rounds the supplied value to two decimal places of accuracy.
 */
function roundScore(score: number): number {
  return Number(Math.round(score * 100) / 100);
}

/**
 * Compares two SiteEngagementDetails objects based on |sortKey|.
 * @param sortKey The name of the property to sort by.
 * @return A negative number if |a| should be ordered before |b|, a
 *     positive number otherwise.
 */
function compareTableItem(
    sortKey: string, a: {[k: string]: any}, b: {[k: string]: any}): number {
  const val1 = a[sortKey];
  const val2 = b[sortKey];

  // Compare the hosts of the origin ignoring schemes.
  if (sortKey === 'origin') {
    return new URL(val1.url).host > new URL(val2.url).host ? 1 : -1;
  }

  if (sortKey === 'baseScore' || sortKey === 'bonusScore' ||
      sortKey === 'totalScore') {
    return val1 - val2;
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
}


export class SiteEngagementAppElement extends CustomElement {
  static get is() {
    return 'site-engagement-app';
  }

  static override get template() {
    return getTemplate();
  }

  private engagementTableBody: HTMLElement|null = null;
  private info: SiteEngagementDetails[]|null = null;
  engagementDetailsProvider: SiteEngagementDetailsProviderInterface =
      SiteEngagementDetailsProvider.getRemote();
  private updateInterval: number|null = null;
  private showWebUiPages: boolean = false;
  private sortKey: string = 'totalScore';
  private sortReverse: boolean = true;
  private whenPopulatedResolver: PromiseResolver<void> = new PromiseResolver();

  connectedCallback() {
    const engagementTableHeader =
        this.getRequiredElement('#engagement-table-header');
    this.engagementTableBody =
        this.getRequiredElement('#engagement-table-body');

    const headers = engagementTableHeader.children;
    for (let i = 0; i < headers.length; i++) {
      headers[i]!.addEventListener('click', e => {
        const target = e.target as HTMLElement;
        const newSortKey = target.getAttribute('sort-key');
        assert(newSortKey);
        if (this.sortKey === newSortKey) {
          this.sortReverse = !this.sortReverse;
        } else {
          this.sortKey = newSortKey;
          this.sortReverse = false;
        }
        const oldSortColumn = this.getRequiredElement('.sort-column');
        oldSortColumn.classList.remove('sort-column');
        target.classList.add('sort-column');
        target.toggleAttribute('sort-reverse', this.sortReverse);
        this.renderTable();
      });
    }

    const showWebUiPagesCheckbox =
        this.getRequiredElement<HTMLInputElement>('#show-webui-pages-checkbox');
    showWebUiPagesCheckbox.addEventListener(
        'change',
        () => this.handleShowWebUiPages(showWebUiPagesCheckbox.checked));

    this.updateEngagementTable();
    this.enableAutoupdate();
  }

  /**
   * Creates a single row in the engagement table.
   * @param info The info to create the row from.
   */
  private createRow(info: SiteEngagementDetails): HTMLElement {
    const originCell = document.createElement('td');
    originCell.classList.add('origin-cell');
    originCell.textContent = info.origin.url;

    const baseScoreInput = document.createElement('input');
    baseScoreInput.classList.add('base-score-input');
    baseScoreInput.addEventListener('focus', () => this.disableAutoupdate());
    baseScoreInput.addEventListener('blur', () => this.enableAutoupdate());
    baseScoreInput.value = String(info.baseScore);

    const baseScoreCell = document.createElement('td');
    baseScoreCell.classList.add('base-score-cell');
    baseScoreCell.appendChild(baseScoreInput);

    const bonusScoreCell = document.createElement('td');
    bonusScoreCell.classList.add('bonus-score-cell');
    bonusScoreCell.textContent = String(info.installedBonus);

    const totalScoreCell = document.createElement('td');
    totalScoreCell.classList.add('total-score-cell');
    totalScoreCell.textContent = String(info.totalScore);

    const engagementBar = document.createElement('div');
    engagementBar.classList.add('engagement-bar');
    engagementBar.style.width = (info.totalScore * 4) + 'px';

    const engagementBarCell = document.createElement('td');
    engagementBarCell.classList.add('engagement-bar-cell');
    engagementBarCell.appendChild(engagementBar);

    const row = document.createElement('tr');
    row.appendChild(originCell);
    row.appendChild(baseScoreCell);
    row.appendChild(bonusScoreCell);
    row.appendChild(totalScoreCell);
    row.appendChild(engagementBarCell);

    baseScoreInput.addEventListener(
        'change',
        (e: Event) =>
            this.handleBaseScoreChange(info.origin, engagementBar, e));

    return row;
  }

  disableAutoupdate() {
    if (this.updateInterval) {
      clearInterval(this.updateInterval);
    }
    this.updateInterval = null;
  }

  private enableAutoupdate() {
    if (this.updateInterval) {
      clearInterval(this.updateInterval);
    }
    this.updateInterval = setInterval(() => this.updateEngagementTable(), 5000);
  }

  /**
   * Sets the base engagement score when a score input is changed.
   * Resets the length of engagement-bar-cell to match the new score.
   * Also resets the update interval.
   * @param origin The origin of the engagement score to set.
   */
  private handleBaseScoreChange(origin: Url, barCell: HTMLElement, e: Event) {
    const baseScoreInput = e.target as HTMLInputElement;
    this.engagementDetailsProvider.setSiteEngagementBaseScoreForUrl(
        origin, parseFloat(baseScoreInput.value));
    barCell.style.width = (parseFloat(baseScoreInput.value) * 4) + 'px';
    baseScoreInput.blur();
    this.enableAutoupdate();
  }

  /**
   * Adds a new origin with the given base score.
   * @param originInput The text input containing the origin to add.
   * @param scoreInput The text input containing the score to add.
   */
  private handleAddOrigin(
      originInput: HTMLInputElement, scoreInput: HTMLInputElement) {
    try {
      // Validate the URL. If we don't validate here, IPC will kill this
      // renderer on invalid URLs. Other checks like scheme are done on the
      // browser side.
      new URL(originInput.value);
    } catch {
      return;
    }
    const origin: Url = {url: originInput.value};
    const score = parseFloat(scoreInput.value);

    this.engagementDetailsProvider.setSiteEngagementBaseScoreForUrl(
        origin, score);
    scoreInput.blur();
    this.updateEngagementTable();
    this.enableAutoupdate();
  }

  /**
   * Show chrome:// and chrome-untrusted:// pages.
   */
  private handleShowWebUiPages(show: boolean) {
    this.showWebUiPages = show;
    this.renderTable();
  }

  /**
   * Remove all rows from the engagement table.
   */
  private clearTable() {
    assert(this.engagementTableBody);
    this.engagementTableBody.innerHTML = window.trustedTypes!.emptyHTML;
  }

  /**
   * Sort the engagement info based on |sortKey| and |sortReverse|.
   */
  private sortInfo() {
    assert(this.info);
    this.info.sort((a, b) => {
      return (this.sortReverse ? -1 : 1) * compareTableItem(this.sortKey, a, b);
    });
  }

  /**
   * Regenerates the engagement table from |info|.
   */
  private renderTable() {
    this.clearTable();
    this.sortInfo();

    assert(this.info);
    this.info.forEach((info) => {
      if (!this.showWebUiPages &&
          (info.origin.url.startsWith('chrome://') ||
           info.origin.url.startsWith('chrome-untrusted://'))) {
        return;
      }

      // Round all scores to 2 decimal places.
      info.baseScore = roundScore(info.baseScore);
      info.installedBonus = roundScore(info.installedBonus);
      info.totalScore = roundScore(info.totalScore);

      assert(this.engagementTableBody);
      this.engagementTableBody.appendChild(this.createRow(info));
    });

    // Add another row for adding a new origin.
    const originInput = document.createElement('input');
    originInput.classList.add('origin-input');
    originInput.addEventListener('focus', () => this.disableAutoupdate());
    originInput.addEventListener('blur', () => this.enableAutoupdate());
    originInput.value = 'http://example.com';

    const originCell = document.createElement('td');
    originCell.appendChild(originInput);

    const baseScoreInput = document.createElement('input');
    baseScoreInput.classList.add('base-score-input');
    baseScoreInput.addEventListener('focus', () => this.disableAutoupdate());
    baseScoreInput.addEventListener('blur', () => this.enableAutoupdate());
    baseScoreInput.value = '0';

    const baseScoreCell = document.createElement('td');
    baseScoreCell.classList.add('base-score-cell');
    baseScoreCell.appendChild(baseScoreInput);

    const addButton = document.createElement('button');
    addButton.textContent = 'Add';
    addButton.classList.add('add-origin-button');

    const buttonCell = document.createElement('td');
    buttonCell.colSpan = 2;
    buttonCell.classList.add('base-score-cell');
    buttonCell.appendChild(addButton);

    const row = document.createElement('tr');
    row.appendChild(originCell);
    row.appendChild(baseScoreCell);
    row.appendChild(buttonCell);
    addButton.addEventListener(
        'click', () => this.handleAddOrigin(originInput, baseScoreInput));

    assert(this.engagementTableBody);
    this.engagementTableBody.appendChild(row);
  }

  /**
   * Retrieve site engagement info and render the engagement table.
   */
  private async updateEngagementTable() {
    // Populate engagement table.
    this.info =
        (await this.engagementDetailsProvider.getSiteEngagementDetails()).info;
    this.renderTable();
    this.whenPopulatedResolver.resolve();
  }

  whenPopulatedForTest() {
    return this.whenPopulatedResolver.promise;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-engagement-app': SiteEngagementAppElement;
  }
}

customElements.define(SiteEngagementAppElement.is, SiteEngagementAppElement);
