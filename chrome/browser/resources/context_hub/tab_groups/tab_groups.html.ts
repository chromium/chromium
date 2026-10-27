// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabInfo} from '../context_hub.mojom-webui.js';

import {DEFECT_CATEGORIES, UNGROUPED_INDEX} from './tab_groups.js';
import type {TabGroupsElement} from './tab_groups.js';

export function getHtml(this: TabGroupsElement) {
  // clang-format off
  return html`
    <main id="tab-groups-view">
      ${this.isGrouping_ ? html`
        <div class="card skeleton-card" aria-busy="true">
          <h2 class="card-title">
            ${this.isRegrouping_ ? 'Regrouping your tabs…' :
                                   'Grouping your tabs…'}
          </h2>
          ${[0, 1, 2].map(() => html`
            <div class="skeleton-group">
              <div class="skeleton skeleton-title"></div>
              <div class="skeleton-row">
                <div class="skeleton skeleton-icon"></div>
                <div class="skeleton skeleton-line"></div>
              </div>
              <div class="skeleton-row">
                <div class="skeleton skeleton-icon"></div>
                <div class="skeleton skeleton-line short"></div>
              </div>
            </div>
          `)}
        </div>
      ` : ''}

      ${!this.isGrouping_ && !this.isGrouped_ ? html`
        <div class="card" id="landing-card">
          <h2 class="card-title">Stay organized with tab groups</h2>
          <p class="landing-text">
            ${this.tabs_.length} open tabs are not in a group. Get
            suggestions for grouping them, then review and edit the
            suggestions before any groups are created.
          </p>
          ${this.tabs_.length > 0 ? html`
            <div class="group-tabs-list landing-tabs">
              ${this.tabs_.map(tab => html`
                <button class="tab-row" data-id="${tab.id}"
                    title="${tab.url}" @click="${this.onTabClick_}">
                  <span class="favicon" style="background-image: ${
                      getFaviconForPageURL(tab.url, false)}"></span>
                  <span class="tab-title">${tab.title}</span>
                </button>
              `)}
            </div>
          ` : ''}
          <cr-button class="action-button" id="check-now-button"
              ?disabled="${this.isBusy_()}"
              @click="${this.onGroupTabsClick_}">
            Check now
          </cr-button>
        </div>
      ` : ''}

      ${!this.isGrouping_ && this.isGrouped_ ? html`
        <div class="card" id="suggestions-card">
          <h2 class="card-title">
            ${this.groups_.length > 0 ?
                `${this.groups_.length} tab groups suggested` :
                'No suggested groups left'}
          </h2>
          ${this.responseText_ ? html`
            <p class="response-text">${this.responseText_}</p>
          ` : ''}
          <div class="groups-container">
            ${this.groups_.map((group, index) => html`
              <div class="group-card ${this.dropTargetIndex_ === index ?
                                           'drop-target' : ''}"
                  data-group="${index}"
                  @dragover="${this.onCardDragover_}"
                  @dragleave="${this.onCardDragleave_}"
                  @drop="${this.onCardDrop_}">
                <div class="group-header"
                    draggable="${this.editingGroupIndex_ !== index}"
                    data-group="${index}"
                    @dragstart="${this.onGroupDragstart_}"
                    @dragend="${this.onDragend_}">
                  <span class="group-color-dot"></span>
                  ${this.editingGroupIndex_ === index ? html`
                    <cr-input id="rename-input" class="group-rename"
                        aria-label="Group name"
                        .value="${this.editingLabel_}"
                        @value-changed="${this.onRenameValueChanged_}"
                        @keydown="${this.onRenameKeydown_}"
                        @blur="${this.onRenameBlur_}">
                    </cr-input>
                  ` : html`
                    <span class="group-label">${group.label}</span>
                    <span class="group-count">${group.tabs.length} tabs</span>
                    <cr-icon-button class="edit-group-btn"
                        iron-icon="cr:edit-filled"
                        title="Rename group"
                        data-index="${index}"
                        @click="${this.onEditGroupClick_}">
                    </cr-icon-button>
                    <cr-icon-button class="dismiss-group-btn"
                        iron-icon="cr:close"
                        title="Dismiss group"
                        data-index="${index}"
                        @click="${this.onDismissGroupClick_}">
                    </cr-icon-button>
                  `}
                </div>
                <div class="group-tabs-list">
                  ${group.tabs.map(tab => html`
                    <div class="tab-item" draggable="true"
                        data-id="${tab.id}" data-group="${index}"
                        @dragstart="${this.onTabDragstart_}"
                        @dragend="${this.onDragend_}">
                      <button class="tab-row" data-id="${tab.id}"
                          title="${tab.url}" @click="${this.onTabClick_}">
                        <span class="favicon" style="background-image: ${
                            getFaviconForPageURL(tab.url, false)}"></span>
                        <span class="tab-title">${tab.title}</span>
                      </button>
                      <cr-icon-button class="move-tab-btn"
                          iron-icon="cr:more-vert"
                          title="Move to another group"
                          data-id="${tab.id}" data-group="${index}"
                          @click="${this.onMoveTabClick_}">
                      </cr-icon-button>
                    </div>
                  `)}
                </div>
              </div>
            `)}
          </div>
        </div>

        ${this.ungroupedTabs_.length > 0 ? html`
          <div class="card ${this.dropTargetIndex_ === UNGROUPED_INDEX ?
                                 'drop-target' : ''}"
              id="ungrouped-card" data-group="${UNGROUPED_INDEX}"
              @dragover="${this.onCardDragover_}"
              @dragleave="${this.onCardDragleave_}"
              @drop="${this.onCardDrop_}">
            <cr-expand-button
                ?expanded="${this.ungroupedExpanded_}"
                @expanded-changed="${this.onUngroupedExpandedChanged_}">
              <span class="card-title">
                Not grouped (${this.ungroupedTabs_.length})
              </span>
            </cr-expand-button>
            <cr-collapse ?opened="${this.ungroupedExpanded_}">
              <div class="group-tabs-list">
                ${this.ungroupedTabs_.map(tab => html`
                  <div class="tab-item" draggable="true"
                      data-id="${tab.id}" data-group="${UNGROUPED_INDEX}"
                      @dragstart="${this.onTabDragstart_}"
                      @dragend="${this.onDragend_}">
                    <button class="tab-row" data-id="${tab.id}"
                        title="${tab.url}" @click="${this.onTabClick_}">
                      <span class="favicon" style="background-image: ${
                          getFaviconForPageURL(tab.url, false)}"></span>
                      <span class="tab-title">${tab.title}</span>
                    </button>
                    ${this.groups_.length > 0 ? html`
                      <cr-icon-button class="move-tab-btn"
                          iron-icon="cr:more-vert"
                          title="Move to a group"
                          data-id="${tab.id}" data-group="${UNGROUPED_INDEX}"
                          @click="${this.onMoveTabClick_}">
                      </cr-icon-button>
                    ` : ''}
                  </div>
                `)}
              </div>
            </cr-collapse>
          </div>
        ` : ''}
      ` : ''}

      ${this.isGrouped_ ? html`
        <div class="card" id="prompt-card">
          <label class="prompt-label" for="change-input">
            Describe any changes
          </label>
          <cr-textarea id="change-input" autogrow rows="2"
              placeholder="Ex: Group by task, split large groups, etc."
              ?disabled="${this.isBusy_()}"
              .value="${this.inputValue_}"
              @value-changed="${this.onInputValueChanged_}"
              @keydown="${this.onInputKeydown_}">
          </cr-textarea>
          <div class="footer-actions">
            <cr-button id="clear-button" ?disabled="${this.isBusy_()}"
                @click="${this.onClearClick_}">
              Clear
            </cr-button>
            <span class="spacer"></span>
            <cr-button id="undo-button"
                ?disabled="${this.isBusy_() || !this.canUndo_}"
                @click="${this.onUndoClick_}">
              Undo
            </cr-button>
            <cr-button class="action-button" id="primary-button"
                ?disabled="${this.isBusy_() ||
                    (!this.hasPendingPrompt_() && this.groups_.length === 0)}"
                @click="${this.onPrimaryActionClick_}">
              ${this.hasPendingPrompt_() ? 'Update' : 'Create groups'}
            </cr-button>
          </div>
          <div class="disclaimer">
            <span>
              Suggestions are generated by AI and may be inaccurate. Tab titles
              and URLs are sent to Google to generate suggestions.
            </span>
            <div class="canvas-feedback-actions">
              <cr-icon-button
                  iron-icon="${this.getCanvasThumbsUpIcon_()}"
                  title="Like grouping"
                  aria-pressed="${this.canvasFeedbackLiked_ === true}"
                  @click="${this.onCanvasThumbsUpClick_}">
              </cr-icon-button>
              <cr-icon-button
                  iron-icon="${this.getCanvasThumbsDownIcon_()}"
                  title="Dislike grouping"
                  aria-pressed="${this.canvasFeedbackLiked_ === false}"
                  @click="${this.onCanvasThumbsDownClick_}">
              </cr-icon-button>
            </div>
          </div>
        </div>
      ` : ''}

      ${this.statusMessage_ ? html`
        <p class="status" role="status">${this.statusMessage_}</p>
      ` : ''}

      ${this.confirmedGroupSummaries_.length > 0 ? html`
        <div class="card" id="confirmed-card">
          <div class="confirmed-header-row">
            <cr-expand-button class="saved-group-expand"
                ?expanded="${this.confirmedExpanded_}"
                @expanded-changed="${this.onConfirmedExpandedChanged_}">
              <span class="card-title">
                Your tab groups (${this.confirmedGroupSummaries_.length})
              </span>
            </cr-expand-button>
            <cr-button id="ungroup-all-confirmed-groups-button"
                title="Ungroup all confirmed groups"
                @click="${this.onUngroupAllConfirmedGroupsClick_}">
              Ungroup All
            </cr-button>
          </div>
          <cr-collapse ?opened="${this.confirmedExpanded_}">
            <div class="groups-container">
              ${this.confirmedGroupSummaries_.map((group, index) => html`
                <div class="group-card">
                  <div class="saved-group-top-row">
                    <cr-expand-button
                        class="saved-group-expand"
                        data-index="${index}"
                        ?expanded="${this.isConfirmedGroupExpanded_(group)}"
                        @expanded-changed="${this.onGroupExpandedChanged_}">
                      <div class="group-header">
                        <span class="group-color-dot"></span>
                        <span class="group-label">${group.label}</span>
                        <span class="group-count">
                          ${group.tabs.length} tabs
                        </span>
                      </div>
                    </cr-expand-button>
                    <div class="saved-group-actions">
                      <cr-icon-button class="close-one-group-btn"
                          iron-icon="cr:close"
                          title="Close confirmed group from window"
                          data-guid="${group.savedGuid?.value}"
                          @click="${this.onCloseConfirmedGroupClick_}">
                      </cr-icon-button>
                      <cr-icon-button class="ungroup-one-group-btn"
                          iron-icon="cr:open-in-new"
                          title="Ungroup confirmed group"
                          data-guid="${group.savedGuid?.value}"
                          @click="${this.onUngroupConfirmedGroupClick_}">
                      </cr-icon-button>
                    </div>
                  </div>
                  <cr-collapse
                      ?opened="${this.isConfirmedGroupExpanded_(group)}">
                    <div class="group-tabs-list">
                      ${group.tabs.map((tab: TabInfo) => html`
                        <div class="tab-row readonly-tab" title="${tab.url}">
                          <span class="favicon" style="background-image: ${
                              getFaviconForPageURL(tab.url, false)}"></span>
                          <span class="tab-title">${tab.title}</span>
                        </div>
                      `)}
                    </div>
                  </cr-collapse>
                </div>
              `)}
            </div>
          </cr-collapse>
        </div>
      ` : ''}
    </main>

    <cr-action-menu id="moveMenu" role-description="Move to"
        @close="${this.onMoveMenuClose_}">
      ${this.getMoveTargets_().map(target => html`
        <button class="dropdown-item" data-group="${target.index}"
            @click="${this.onMoveToClick_}">
          ${target.label}
        </button>
      `)}
    </cr-action-menu>

    ${this.feedbackDialogOpen_ ? html`
      <cr-dialog id="feedback-dialog"
          @close="${this.onFeedbackDialogClose_}"
          show-on-attach>
        <div slot="title" class="feedback-dialog-title">
          <span>Evaluate Tab Grouping</span>
          <span class="dialog-subtitle">
            (${this.feedbackLiked_ ? 'Positive Experience' :
                'Needs Improvement'})
          </span>
        </div>
        <div slot="body" class="feedback-dialog-body">
          <div class="feedback-section">
            <div class="feedback-section-header">
              <span class="feedback-section-label">
                Quality Score (1–10)
              </span>
              <span class="rating-grade-pill ${
                  this.getRatingTierClass_(this.feedbackRating_ ?? 10)}">
                ${this.getRatingTierLabel_(this.feedbackRating_ ?? 10)}
              </span>
            </div>
            <div class="rating-chips-row" role="radiogroup"
                aria-label="Quality Score from 1 to 10">
              ${[1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map(item => html`
                <button type="button" role="radio"
                    class="rating-chip ${this.getRatingTierClass_(item)} ${
                        this.feedbackRating_ === item ? 'selected' : ''}"
                    data-score="${item}"
                    aria-label="Score ${item}: ${
                        this.getRatingTierLabel_(item)}"
                    aria-checked="${this.feedbackRating_ === item}"
                    @click="${this.onRatingChipClick_}">
                  ${item}
                </button>
              `)}
            </div>
          </div>

          <div class="feedback-section">
            <div class="feedback-section-header">
              <span class="feedback-section-label">
                Defect Category
                ${!this.feedbackLiked_ ?
                    html`<span class="required-indicator">*</span>` : ''}
              </span>
            </div>
            <select class="defect-select"
                aria-label="Defect Category"
                .value="${this.feedbackDefect_}"
                @change="${this.onDefectChange_}">
              <option value="" disabled ?selected="${!this.feedbackDefect_}">
                -- Select Defect Category --
              </option>
              ${DEFECT_CATEGORIES.map(item => html`
                <option value="${item}"
                    ?selected="${this.feedbackDefect_ === item}">
                  ${item}
                </option>
              `)}
            </select>
          </div>

          <div class="feedback-section">
            <cr-textarea id="feedback-comments"
                label="Notes & Observations (Optional)"
                placeholder="Explain what worked well or what was incorrect..."
                .value="${this.feedbackComments_}"
                @value-changed="${this.onFeedbackCommentsValueChanged_}">
            </cr-textarea>
          </div>

          <div class="feedback-section">
            <cr-input id="rater-input"
                label="Rater / Evaluator (Optional LDAP or Name)"
                placeholder="e.g. username"
                .value="${this.raterName_}"
                @value-changed="${this.onRaterNameValueChanged_}">
            </cr-input>
          </div>

          <div class="sheet-paste-hint">
            Click <strong>Copy Row for Sheet</strong>, then press
            <code>Paste</code> in Column A of the Google Sheet.
          </div>
        </div>
        <div slot="button-container">
          ${this.copiedRowSuccess_ ? html`
            <span class="copy-success-pill">
              ✓ Copied row for Sheet!
            </span>
          ` : ''}
          <cr-button class="cancel-button"
              @click="${this.onCloseFeedbackDialogClick_}">
            ${this.copiedRowSuccess_ ? 'Done' : 'Cancel'}
          </cr-button>
          <cr-button class="action-button"
              id="copy-row-button"
              ?disabled="${!this.canSubmitFeedback_()}"
              @click="${this.onCopyForSheetClick_}">
            ${this.copiedRowSuccess_ ? 'Copy Row Again' :
                'Copy Row for Sheet'}
          </cr-button>
        </div>
      </cr-dialog>
    ` : ''}
  `;
  // clang-format on
}
