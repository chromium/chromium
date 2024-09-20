// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DownloadsItemElement} from './item.js';

export function getHtml(this: DownloadsItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="date" role="heading" aria-level="2">${this.computeDate_()}</div>

<div id="content" @dragstart="${this.onDragStart_}"
    class="${this.computeClass_()}" focus-row-container>
  <div id="main-content">
    <div id="file-icon-wrapper" class="icon-wrapper" role="img"
        aria-label="${this.computeIconAriaLabel_()}"
        aria-hidden="${this.computeIconAriaHidden_()}">
      <img class="icon" id="file-icon" alt="" ?hidden="${!this.useFileIcon_}"
          icon-color="${this.computeIconColor_()}">
      <cr-icon class="icon" ?hidden="${this.useFileIcon_}"
          .icon="${this.computeIcon_()}"
          icon-color="${this.computeIconColor_()}">
      </cr-icon>
    </div>

    <div id="details">
      <div id="title-area" role="gridcell"><!--
        Can't have any line breaks.
        --><a is="action-link" id="file-link"
            href="${this.data?.url?.url || ''}"
            @click="${this.onFileLinkClick_}" focus-row-control
            focus-type="fileLink"
            ?hidden="${!this.shouldLinkFilename_}"><!-- No line break
          -->${this.data?.fileName || ''}<!-- No line break
        --></a><!--
        Before #name.
        --><span id="name"
            ?hidden="${this.shouldLinkFilename_}"><!-- No line break
          -->${this.data?.fileName || ''}</span>
        <span id="tag">${this.computeTag_()}</span>
      </div>

      <div role="gridcell">
        <div id="referrer-url"
            ?hidden="${!this.shouldShowReferrerUrl_()}">
          <!-- Text populated dynamically -->
        </div>
        <a id="url" ?hidden="${this.showReferrerUrl_}" target="_blank"
          @click="${this.onUrlClick_}" focus-row-control
          focus-type="url">${this.getDisplayUrlStr_()}</a>
      </div>

      <div class="description" role="gridcell"
          description-color="${this.iconAndDescriptionColor_()}"
          ?hidden="${!this.computeDescriptionVisible_()}">
        ${this.computeDescription_()}
      </div>

      <div class="description" role="gridcell"
          ?hidden="${!this.computeSecondLineVisible_()}">
        $i18n{asyncScanningDownloadDescSecond}
      </div>

      ${this.showProgress_ ? html`
        <div role="gridcell">
          <cr-progress id="progress"
              .indeterminate="${this.isIndeterminate_()}"
              .value="${this.data?.percent || 0}">
          </cr-progress>
        </div>` : ''}

      <div id="safe" class="controls" ?hidden="${this.isDangerous_}">
        <span role="gridcell" ?hidden="${!this.showDeepScan_}">
          <cr-button @click="${this.onDeepScanClick_}" id="deepScan"
              class="action-button" focus-row-control focus-type="open">
            ${this.computeDeepScanControlText_()}
          </cr-button>
        </span>
      </div>
      <div id="controlled-by" ?hidden="${this.isDangerous_}"><!--
        Text populated dynamically.
      --></div>
    </div>
    <div class="more-options">
      <!-- Menu and/or quick action(s). -->
      <div role="gridcell" id="action-icon-buttons">
        <cr-icon-button id="copy-download-link" iron-icon="downloads:link"
            ?hidden="${!this.computeShowCopyDownloadLink_()}"
            title="$i18n{controlCopyDownloadLink}"
            aria-label="$i18n{controlCopyDownloadLink}"
            @click="${this.onCopyDownloadLinkClick_}"
            focus-row-control focus-type="copyDownloadLink">
        </cr-icon-button>
        <cr-icon-button id="more-actions" iron-icon="cr:more-vert"
            ?hidden="${!this.computeShowActionMenu_()}"
            class="dropdown-trigger" title="$i18n{moreActions}"
            @click="${this.onMoreActionsClick_}" aria-haspopup="menu"
            focus-row-control focus-type="actionMenuButton">
        </cr-icon-button>
        <cr-icon-button id="quick-show-in-folder" class="icon-folder-open"
            ?hidden="${!this.computeShowQuickShow_()}"
            title="${this.data?.showInFolderText || ''}"
            aria-label="${this.data?.showInFolderText || ''}"
            @click="${this.onShowClick_}"
            focus-row-control focus-type="quickShow">
        </cr-icon-button>
        <cr-icon-button id="quick-remove" class="icon-clear"
            ?hidden="${!this.computeShowQuickRemove_()}"
            title="$i18n{controlDeleteFromHistory}"
            aria-label="$i18n{controlDeleteFromHistory}"
            @click="${this.onQuickRemoveClick_}"
            focus-row-control focus-type="quickRemove">
        </cr-icon-button>
      </div>
      <cr-action-menu id="more-actions-menu"
          role-description="$i18n{actionMenuDescription}">
        <button class="dropdown-item" @click="${this.onShowClick_}"
            ?hidden="${!this.computeHasShowInFolderLink_()}"
            id="show-in-folder">
          ${this.data?.showInFolderText || ''}
        </button>
        <button class="dropdown-item" @click="${this.onPauseOrResumeClick_}"
            ?hidden="${!this.pauseOrResumeText_}" id="pause-or-resume">
          ${this.pauseOrResumeText_}
        </button>
        <button class="dropdown-item" @click="${this.onRemoveClick_}"
            ?hidden="${!this.computeShowRemove_()}" id="remove">
          $i18n{controlDeleteFromHistory}
        </button>
        <button class="dropdown-item" @click="${this.onDiscardDangerousClick_}"
            ?hidden="${!this.computeShowControlsForDangerous_()}"
            id="discard-dangerous">
          $i18n{controlDeleteFromHistory}
        </button>
        <button class="dropdown-item" @click="${this.onRetryClick_}"
            ?hidden="${!this.data?.retry}" id="retry">
          $i18n{controlRetry}
        </button>
        <button class="dropdown-item" @click="${this.onDeepScanClick_}"
            ?hidden="${!this.showDeepScan_}" id="deep-scan">
          $i18n{controlDeepScan}
        </button>
        <button class="dropdown-item" @click="${this.onBypassDeepScanClick_}"
            ?hidden="${!this.showDeepScan_}" id="bypass-deep-scan">
          ${this.computeSaveDangerousLabel_()}
        </button>
        <button class="dropdown-item" @click="${this.onCancelClick_}"
            ?hidden="${!this.showCancel_}" id="cancel">
          $i18n{controlCancel}
        </button>
        <button class="dropdown-item" @click="${this.onOpenAnywayClick_}"
            ?hidden="${!this.showOpenAnyway_}" id="open-anyway">
          $i18n{controlOpenAnyway}
        </button>
        <button class="dropdown-item" @click="${this.onDiscardDangerousClick_}"
            ?hidden="${!this.isReviewable_}" id="reviewable-discard-dangerous">
          $i18n{dangerDiscard}
        </button>
        <button class="dropdown-item" @click="${this.onReviewDangerousClick_}"
            ?hidden="${!this.isReviewable_}" id="review-dangerous">
          $i18n{dangerReview}
        </button>
        <button class="dropdown-item" @click="${this.onSaveDangerousClick_}"
            ?hidden="${!this.computeShowControlsForDangerous_()}"
            id="save-dangerous">
          ${this.computeSaveDangerousLabel_()}
        </button>
      </cr-action-menu>
      <div id="incognito" title="$i18n{inIncognito}"
          ?hidden="${!this.data?.otr}">
      </div>
    </div>
  </div>
  <if expr="_google_chrome">
    ${this.showEsbPromotion ? html`
      <cr-link-row
          id="esb-download-row-promo"
          start-icon="downloads-internal:gshield"
          external
          @click="${this.onEsbPromotionClick_}"
          button-aria-description="$i18n{esbDownloadRowPromoA11y}"
          label="$i18n{esbDownloadRowPromoString}">
      </cr-link-row>` : ''}
  </if>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
