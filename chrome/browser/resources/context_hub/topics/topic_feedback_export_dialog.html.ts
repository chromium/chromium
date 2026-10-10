// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {EXPORT_WINDOW_DAYS} from './topic_feedback_export_dialog.js';
import type {TopicFeedbackExportDialogElement} from './topic_feedback_export_dialog.js';

export function getHtml(this: TopicFeedbackExportDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- Temporary Fishfood Feedback -->
<!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
<!-- strings are added. -->
<cr-dialog id="dialog" show-on-attach @close="${this.onDialogClose_}">
  <div slot="title">
    <div id="title">Send fishfood feedback</div>
    <div id="subtitle">
      Download your ratings and recent browsing as a file, then upload it to
      the feedback form.
    </div>
  </div>
  <div slot="body">
    <section id="windowSection">
      <div class="section-header">
        <div>
          <h2>Browsing history to share</h2>
          ${this.previewState_ === 'loading' ? html`
            <div id="previewLoading" class="caption" role="status">
              Loading…
            </div>
          ` : ''}
          ${this.previewState_ === 'error' ? html`
            <div id="previewError" class="error" role="alert">
              Couldn't load your browsing history. Try again later.
            </div>
          ` : ''}
          ${this.previewState_ === 'loaded' ? html`
            <div id="stats" class="caption">${this.getStatsText_()}</div>
          ` : ''}
        </div>
        <select id="windowSelect" class="md-select" aria-label="Time window"
            @change="${this.onWindowChange_}">
          ${EXPORT_WINDOW_DAYS.map(days => html`
            <option value="${days}" ?selected="${days === this.windowDays_}">
              ${this.getWindowLabel_(days)}
            </option>
          `)}
        </select>
      </div>
    </section>

    <section id="coverageSection">
      <h2>Your ratings</h2>
      <div id="ratingStatus" ?complete="${this.areAllTopicsRated_()}">
        <cr-icon icon="${this.areAllTopicsRated_() ?
            'cr:check-circle' : 'cr:error'}" aria-hidden="true">
        </cr-icon>
        <div id="ratingProgress">${this.getRatingProgressText_()}</div>
      </div>
      ${this.getCoverageTopics_().length > 0 ? html`
        <div class="secondary">
          If you have time, also review these topics on their details page:
        </div>
        <ul id="coverageTopics">
          ${this.getCoverageTopics_().map(topic => html`
            <li ?done="${this.isCoverageTopicDone_(topic)}">
              <cr-icon icon="${this.isCoverageTopicDone_(topic) ?
                  'cr:check-circle' : 'cr:chevron-right'}"
                  aria-hidden="true">
              </cr-icon>
              <a href="#" data-id="${topic.id}"
                  @click="${this.onCoverageTopicClick_}">${topic.title}</a>
              ${this.isCoverageTopicDone_(topic) ? html`
                <span class="secondary">Reviewed</span>
              ` : ''}
            </li>
          `)}
        </ul>
      ` : ''}
      <cr-textarea id="missingTopics"
          label="Missing topics (optional)" autogrow rows="2"
          placeholder="What did you browse that should have been a topic?"
          .value="${this.missingTopics_}"
          @value-changed="${this.onMissingTopicsValueChanged_}">
      </cr-textarea>
    </section>

    <section id="redactionSection">
      <h2>Sites</h2>
      <div class="secondary">
        Uncheck a site to leave it out of the file. Internal and Workspace
        sites are left out by default. Expand a site to leave out single pages.
      </div>
      ${this.previewState_ === 'loaded' && this.getDomains_().length === 0 ?
          html`
        <div id="noDomains" class="secondary">No browsing in this window.</div>
      ` : ''}
      <ul id="domainList">
        ${this.getDomains_().map((domain, index) => html`
          <li class="domain" ?excluded="${this.isDomainExcluded_(domain)}">
            <div class="domain-row">
              <cr-expand-button class="domain-expand" data-index="${index}"
                  ?expanded="${this.isDomainExpanded_(domain)}"
                  @expanded-changed="${this.onDomainExpandedChanged_}">
                <span class="domain-name">${domain.domain}</span>
                <span class="domain-count">
                  ${this.getDomainCountText_(domain)}
                </span>
              </cr-expand-button>
              <cr-checkbox class="domain-checkbox" data-index="${index}"
                  ?checked="${!this.isDomainExcluded_(domain)}"
                  .ariaLabelOverride="Share ${domain.domain}"
                  @checked-changed="${this.onDomainCheckedChanged_}">
              </cr-checkbox>
            </div>
            <cr-collapse ?opened="${this.isDomainExpanded_(domain)}">
              ${this.isDomainExpanded_(domain) ? html`
                <ul class="url-list">
                  ${domain.urls.map(url => html`
                    <li>
                      <cr-checkbox class="url-checkbox" data-url="${url.url}"
                          ?checked="${this.isUrlIncluded_(domain, url)}"
                          ?disabled="${this.isDomainExcluded_(domain)}"
                          @checked-changed="${this.onUrlCheckedChanged_}">
                        <div class="url-text">
                          <span class="url-title">${url.title || url.url}</span>
                          <span class="url">${url.url}</span>
                        </div>
                      </cr-checkbox>
                    </li>
                  `)}
                </ul>
              ` : ''}
            </cr-collapse>
          </li>
        `)}
      </ul>
    </section>

    <section id="privacySection">
      <h2>Privacy</h2>
      <p id="disclosure" class="secondary">
        The file contains your topics, your ratings and comments, and the URLs,
        titles and times of the pages you visited in this window, except what
        you exclude above, plus your Chrome version and experiment state. Only
        the Topics team can access it. It's deleted after 90 days; to have it
        deleted sooner, contact the Topics team.
      </p>
      <cr-checkbox id="acknowledge" ?checked="${this.acknowledged_}"
          @checked-changed="${this.onAcknowledgeCheckedChanged_}">
        I understand what's shared
      </cr-checkbox>
    </section>

    ${this.exportState_ === 'done' ? html`
      <div id="exportDone" class="banner" role="status">
        <span>Downloaded. Upload the file to the feedback form.</span>
        ${this.getFormUrl_() ? html`
          <cr-button id="openForm" @click="${this.onOpenFormClick_}">
            Open feedback form
          </cr-button>
        ` : ''}
      </div>
    ` : ''}
    ${this.exportState_ === 'error' ? html`
      <div id="exportError" class="error" role="alert">
        Couldn't create the file. Try again.
      </div>
    ` : ''}
  </div>
  <div slot="button-container">
    <div id="clearContainer">
      ${this.confirmingClear_ ? html`
        <span id="clearPrompt">Delete all your ratings?</span>
        <cr-button id="cancelClear" @click="${this.onCancelClearClick_}">
          Cancel
        </cr-button>
        <cr-button id="confirmClear" @click="${this.onConfirmClearClick_}">
          Delete
        </cr-button>
      ` : html`
        <cr-button id="clearRatings" @click="${this.onClearClick_}">
          Clear my ratings
        </cr-button>
      `}
    </div>
    <cr-button id="close" class="cancel-button" @click="${this.onCloseClick_}">
      Close
    </cr-button>
    <cr-button id="download" class="action-button"
        ?disabled="${!this.canDownload_()}"
        @click="${this.onDownloadClick_}">
      Download JSON
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
