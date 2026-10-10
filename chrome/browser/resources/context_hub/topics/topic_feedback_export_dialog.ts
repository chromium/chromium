// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, TopicRating} from '../context_hub.mojom-webui.js';
import type {TopicsFeedbackExportDomain, TopicsFeedbackExportOptions, TopicsFeedbackExportPreview, TopicsFeedbackExportUrl} from '../context_hub.mojom-webui.js';

import {getCss} from './topic_feedback_export_dialog.css.js';
import {getHtml} from './topic_feedback_export_dialog.html.js';
import type {TopicFeedback, TopicItem} from './topic_utils.js';

export interface TopicFeedbackExportDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

// The history windows the rater can export, in days.
export const EXPORT_WINDOW_DAYS: readonly number[] = [1, 3, 5, 7, 14];
export const DEFAULT_EXPORT_WINDOW_DAYS = 5;

// How many topics the rater is asked to review in detail.
export const COVERAGE_TOPIC_COUNT = 3;

// Session storage key of the ids of the topics picked for detailed review, so
// that the pick stays the same while the rater goes back and forth between the
// dialog and the topics' details pages.
export const COVERAGE_TOPICS_STORAGE_KEY = 'topicsFeedbackCoverageTopicIds';

// Saves `contents` as a file named `fileName` in the user's downloads.
export type FileDownloader = (contents: string, fileName: string) => void;

function downloadFile(contents: string, fileName: string) {
  const blobUrl =
      URL.createObjectURL(new Blob([contents], {type: 'application/json'}));
  const link = document.createElement('a');
  link.href = blobUrl;
  link.download = fileName;
  link.click();
  setTimeout(() => URL.revokeObjectURL(blobUrl));
}

let fileDownloader: FileDownloader = downloadFile;

export function setFileDownloaderForTesting(downloader: FileDownloader|null) {
  fileDownloader = downloader || downloadFile;
}

// Returns the ids of the topics to ask the rater to review in detail: the
// still-existing `storedIds` from earlier in the session, topped up at random
// from `topicIds` to `count` topics.
export function pickCoverageTopicIds(
    topicIds: string[], storedIds: string[], count = COVERAGE_TOPIC_COUNT,
    random: () => number = Math.random): string[] {
  const picked = [...new Set(storedIds)]
                     .filter(id => topicIds.includes(id))
                     .slice(0, count);
  const remaining = topicIds.filter(id => !picked.includes(id));
  while (picked.length < count && remaining.length > 0) {
    const index = Math.floor(random() * remaining.length);
    picked.push(remaining.splice(index, 1)[0]!);
  }
  return picked;
}

// Returns the name of the downloaded bundle, e.g.
// "topics-feedback-2026-10-07.json".
export function getExportFileName(date: Date): string {
  const pad = (value: number) => String(value).padStart(2, '0');
  const day = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${
      pad(date.getDate())}`;
  return `topics-feedback-${day}.json`;
}

// Whether the rater gave more than a thumbs up/down on the topic, which they
// can only do on its details page.
export function hasDetailedFeedback(feedback: TopicFeedback|undefined):
    boolean {
  return !!feedback && feedback.rating !== TopicRating.kUnrated &&
      (feedback.queryFeedbacks.length > 0 ||
       feedback.rejectedVisits.length > 0 || !!feedback.duplicateOf);
}

function readStoredCoverageTopicIds(): string[] {
  try {
    const stored =
        JSON.parse(sessionStorage.getItem(COVERAGE_TOPICS_STORAGE_KEY) || '[]');
    return Array.isArray(stored) ? stored.map(String) : [];
  } catch {
    return [];
  }
}

// TODO(crbug.com/558572977): Use internationalized (and pluralized) strings
// once GRD strings are added.
function count(value: number, noun: string): string {
  return `${value} ${noun}${value === 1 ? '' : 's'}`;
}

export type ExportPreviewState = 'loading'|'loaded'|'error';
export type ExportState = 'idle'|'exporting'|'done'|'error';

// The fishfood feedback export dialog, opened from the "Send fishfood feedback"
// button of <topics-view>. It previews what the bundle would contain for the
// chosen history window, lets the rater redact domains and URLs,
// and downloads the bundle as JSON for the rater to upload to the feedback
// form. Fires `topic-feedbacks-cleared` once "Clear my ratings" has deleted the
// stored feedback.
//
// Temporary Fishfood Feedback.
export class TopicFeedbackExportDialogElement extends CrLitElement {
  static get is() {
    return 'topic-feedback-export-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topics: {type: Array},
      // Stored fishfood feedback, by topic id.
      feedbacks: {type: Object},
      windowDays_: {type: Number},
      preview_: {type: Object},
      previewState_: {type: String},
      domainExclusions_: {type: Object},
      excludedUrls_: {type: Object},
      expandedDomains_: {type: Object},
      missingTopics_: {type: String},
      acknowledged_: {type: Boolean},
      coverageTopicIds_: {type: Array},
      exportState_: {type: String},
      confirmingClear_: {type: Boolean},
    };
  }

  accessor topics: TopicItem[] = [];
  accessor feedbacks: Map<string, TopicFeedback> = new Map();
  protected accessor windowDays_: number = DEFAULT_EXPORT_WINDOW_DAYS;
  protected accessor preview_: TopicsFeedbackExportPreview|null = null;
  protected accessor previewState_: ExportPreviewState = 'loading';
  // Whether the rater excluded each domain. Domains the rater hasn't changed
  // keep the default from the preview.
  protected accessor domainExclusions_: Map<string, boolean> = new Map();
  protected accessor excludedUrls_: Set<string> = new Set();
  protected accessor expandedDomains_: Set<string> = new Set();
  protected accessor missingTopics_: string = '';
  protected accessor acknowledged_: boolean = false;
  protected accessor coverageTopicIds_: string[] = [];
  protected accessor exportState_: ExportState = 'idle';
  protected accessor confirmingClear_: boolean = false;

  // Ignores previews of windows the rater has since moved away from.
  private previewRequestId_: number = 0;

  override connectedCallback() {
    super.connectedCallback();
    this.fetchPreview_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topics')) {
      this.coverageTopicIds_ = pickCoverageTopicIds(
          this.topics.map(topic => topic.id), readStoredCoverageTopicIds());
      sessionStorage.setItem(
          COVERAGE_TOPICS_STORAGE_KEY, JSON.stringify(this.coverageTopicIds_));
    }
  }

  showModal() {
    this.$.dialog.showModal();
  }

  close() {
    this.$.dialog.close();
  }

  private async fetchPreview_() {
    const requestId = ++this.previewRequestId_;
    this.previewState_ = 'loading';
    try {
      const {preview} =
          await browserProxyFactory.getInstance()
              .handler.getTopicsFeedbackExportPreview(this.windowDays_);
      if (requestId !== this.previewRequestId_) {
        return;
      }
      this.preview_ = preview;
      this.previewState_ = 'loaded';
    } catch (e) {
      if (requestId !== this.previewRequestId_) {
        return;
      }
      console.error('Failed to fetch the topics feedback export preview:', e);
      this.previewState_ = 'error';
    }
  }

  protected onWindowChange_(e: Event) {
    const windowDays = Number((e.target as HTMLSelectElement).value);
    if (windowDays === this.windowDays_) {
      return;
    }
    this.windowDays_ = windowDays;
    this.fetchPreview_();
  }

  // Stats.

  protected getWindowLabel_(days: number): string {
    return `Last ${count(days, 'day')}`;
  }

  protected getStatsText_(): string {
    const stats = this.preview_?.stats;
    if (!stats) {
      return '';
    }
    return `${count(stats.topics, 'topic')} · ${
        count(stats.visits, 'visit')} · ${
        count(stats.unclusteredVisits, 'visit')} not in any topic`;
  }

  protected getRatingProgressText_(): string {
    if (this.areAllTopicsRated_()) {
      return 'You rated every topic.';
    }
    return `You rated ${this.getRatedTopicCount_()} of ${
               count(this.topics.length, 'topic')}. ` +
        'Rate every topic on the Topics page before downloading.';
  }

  private isTopicRated_(topic: TopicItem): boolean {
    const rating = this.feedbacks.get(topic.id)?.rating;
    return rating !== undefined && rating !== TopicRating.kUnrated;
  }

  protected getRatedTopicCount_(): number {
    return this.topics.filter(topic => this.isTopicRated_(topic)).length;
  }

  // Every topic shown at the time of the export must have a thumbs up or down,
  // so that the bundle covers all of them.
  protected areAllTopicsRated_(): boolean {
    return this.getRatedTopicCount_() === this.topics.length;
  }

  // Coverage prompt.

  protected getCoverageTopics_(): TopicItem[] {
    return this.coverageTopicIds_
        .map(id => this.topics.find(topic => topic.id === id))
        .filter((topic): topic is TopicItem => !!topic);
  }

  protected isCoverageTopicDone_(topic: TopicItem): boolean {
    return hasDetailedFeedback(this.feedbacks.get(topic.id));
  }

  protected onCoverageTopicClick_(e: Event) {
    e.preventDefault();
    const id = (e.currentTarget as HTMLElement).dataset['id'];
    if (!id) {
      return;
    }
    const params = new URLSearchParams();
    params.set('id', id);
    browserProxyFactory.getInstance().handler.openTopic({
      topicUrl: `chrome://context-hub/topic_details?${params.toString()}`,
    });
  }

  protected onMissingTopicsValueChanged_(e: CustomEvent<{value: string}>) {
    this.missingTopics_ = e.detail.value;
  }

  // Redaction.

  protected getDomains_(): TopicsFeedbackExportDomain[] {
    return this.preview_?.domains || [];
  }

  protected isDomainExcluded_(domain: TopicsFeedbackExportDomain): boolean {
    return this.domainExclusions_.get(domain.domain) ?? domain.defaultExcluded;
  }

  protected isDomainExpanded_(domain: TopicsFeedbackExportDomain): boolean {
    return this.expandedDomains_.has(domain.domain);
  }

  protected isUrlIncluded_(
      domain: TopicsFeedbackExportDomain,
      url: TopicsFeedbackExportUrl): boolean {
    return !this.isDomainExcluded_(domain) && !this.excludedUrls_.has(url.url);
  }

  protected getExcludedUrlCount_(domain: TopicsFeedbackExportDomain): number {
    return domain.urls.filter(url => this.excludedUrls_.has(url.url)).length;
  }

  protected getDomainCountText_(domain: TopicsFeedbackExportDomain): string {
    const visits = count(domain.visitCount, 'visit');
    if (this.isDomainExcluded_(domain)) {
      return `${visits} · left out`;
    }
    const excludedUrlCount = this.getExcludedUrlCount_(domain);
    return excludedUrlCount > 0 ?
        `${visits} · ${count(excludedUrlCount, 'page')} left out` :
        visits;
  }

  private getDomainAt_(e: Event): TopicsFeedbackExportDomain|undefined {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    return this.getDomains_()[index];
  }

  protected onDomainExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    const domain = this.getDomainAt_(e);
    if (!domain || e.detail.value === this.isDomainExpanded_(domain)) {
      return;
    }
    const expandedDomains = new Set(this.expandedDomains_);
    if (e.detail.value) {
      expandedDomains.add(domain.domain);
    } else {
      expandedDomains.delete(domain.domain);
    }
    this.expandedDomains_ = expandedDomains;
  }

  protected onDomainCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const domain = this.getDomainAt_(e);
    if (!domain || e.detail.value === !this.isDomainExcluded_(domain)) {
      return;
    }
    const domainExclusions = new Map(this.domainExclusions_);
    domainExclusions.set(domain.domain, !e.detail.value);
    this.domainExclusions_ = domainExclusions;
  }

  protected onUrlCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const url = (e.currentTarget as HTMLElement).dataset['url'];
    if (url === undefined || e.detail.value === !this.excludedUrls_.has(url)) {
      return;
    }
    const excludedUrls = new Set(this.excludedUrls_);
    if (e.detail.value) {
      excludedUrls.delete(url);
    } else {
      excludedUrls.add(url);
    }
    this.excludedUrls_ = excludedUrls;
  }

  // Disclosure.

  protected onAcknowledgeCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.acknowledged_ = e.detail.value;
  }

  // Actions.

  protected canDownload_(): boolean {
    return this.previewState_ === 'loaded' && this.areAllTopicsRated_() &&
        this.acknowledged_ && this.exportState_ !== 'exporting';
  }

  getExportOptionsForTesting(): TopicsFeedbackExportOptions {
    return this.getExportOptions_();
  }

  private getExportOptions_(): TopicsFeedbackExportOptions {
    const domains = this.getDomains_();
    const excludedDomains =
        domains.filter(domain => this.isDomainExcluded_(domain));
    return {
      windowDays: this.windowDays_,
      excludedDomains: excludedDomains.map(domain => domain.domain),
      // URLs of excluded domains are redacted anyway.
      excludedUrls: domains.filter(domain => !this.isDomainExcluded_(domain))
                        .flatMap(domain => domain.urls)
                        .map(url => url.url)
                        .filter(url => this.excludedUrls_.has(url)),
      missingTopics: this.missingTopics_.trim(),
    };
  }

  protected async onDownloadClick_() {
    if (!this.canDownload_()) {
      return;
    }
    this.exportState_ = 'exporting';
    const options = this.getExportOptions_();
    try {
      const {jsonBundle} = await browserProxyFactory.getInstance()
                               .handler.generateTopicsFeedbackBundle(options);
      fileDownloader(jsonBundle, getExportFileName(new Date()));
      this.exportState_ = 'done';
    } catch (e) {
      console.error('Failed to generate the topics feedback bundle:', e);
      this.exportState_ = 'error';
    }
  }

  protected getFormUrl_(): string {
    return loadTimeData.valueExists('kTopicsFeedbackFormUrl') ?
        loadTimeData.getString('kTopicsFeedbackFormUrl') :
        '';
  }

  protected onOpenFormClick_() {
    const formUrl = this.getFormUrl_();
    if (formUrl) {
      OpenWindowProxyImpl.getInstance().openUrl(formUrl);
    }
  }

  protected onClearClick_() {
    this.confirmingClear_ = true;
  }

  protected onCancelClearClick_() {
    this.confirmingClear_ = false;
  }

  protected async onConfirmClearClick_() {
    this.confirmingClear_ = false;
    await browserProxyFactory.getInstance().handler.clearTopicFeedbacks();
    sessionStorage.removeItem(COVERAGE_TOPICS_STORAGE_KEY);
    this.fire('topic-feedbacks-cleared');
  }

  protected onCloseClick_() {
    this.close();
  }

  protected onDialogClose_() {
    this.fire('export-dialog-close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-feedback-export-dialog': TopicFeedbackExportDialogElement;
  }
}

customElements.define(
    TopicFeedbackExportDialogElement.is, TopicFeedbackExportDialogElement);
