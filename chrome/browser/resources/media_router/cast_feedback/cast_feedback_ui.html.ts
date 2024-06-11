// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CastFeedbackUiElement} from './cast_feedback_ui.js';
import {FeedbackType} from './cast_feedback_ui.js';

export function getHtml(this: CastFeedbackUiElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div>
  <div id="header-banner"></div>
  <div id="form">
    <div id="header">
      <div class="h1">$i18n{header}</div>
      <div id="description" class="informative">
        $i18nRaw{formDescription}
      </div>
      <div id="required-legend"
           class="informative required-message">
        * $i18n{required}
      </div>
    </div>
    <div id="feedback-type-toggle">
      <div class="h2">$i18n{typeQuestion}</div>
      <cr-radio-group .selected="${this.feedbackType_}"
          @selected-changed="${this.onFeedbackTypeChanged_}">
        <cr-radio-button name="${FeedbackType.BUG}">
          $i18n{typeBugOrError}
        </cr-radio-button>
        <cr-radio-button name="${FeedbackType.FEATURE_REQUEST}">
          $i18n{typeFeatureRequest}
        </cr-radio-button>
        <cr-radio-button name="${FeedbackType.MIRRORING_QUALITY}">
          $i18n{typeProjectionQuality}
        </cr-radio-button>
        <cr-radio-button name="${FeedbackType.DISCOVERY}">
          $i18n{typeDiscovery}
        </cr-radio-button>
        <cr-radio-button name="${FeedbackType.OTHER}">
          $i18n{typeOther}
        </cr-radio-button>
      </cr-radio-group>
    </div>
    <div class="question" ?hidden="${!this.showDefaultSection_()}">
      <div class="h2">
        $i18n{prompt}
        <span class="required-message"
            ?hidden="${this.sufficientFeedback_}">*</span>
      </div>
      <textarea placeholder="$i18n{yourAnswer}" rows="8" cols="60"
          .value="${this.comments_}" @input="${this.onCommentsInput_}">
      </textarea>
    </div>
    ${this.showMirroringQualitySection_() ? html`
      <div class="question">
        <div class="h2">
          $i18n{mirroringQualitySubheading}
          <span class="required-message"
              ?hidden="${this.sufficientFeedback_}">*</span>
        </div>
        <div class="question-part">
          <div class="h3">$i18n{videoSmoothness}</div>
          <cr-radio-group .selected="${this.videoSmoothness_}"
              @selected-changed="${this.onVideoSmoothnessChanged_}">
            <cr-radio-button name="1">
              1 ($i18n{videoFreezes})
            </cr-radio-button>
            <cr-radio-button name="2">
              2 ($i18n{videoJerky})
            </cr-radio-button>
            <cr-radio-button name="3">
              3 ($i18n{videoStutter})
            </cr-radio-button>
            <cr-radio-button name="4">
              4 ($i18n{videoSmooth})
            </cr-radio-button>
            <cr-radio-button name="5">
              5 ($i18n{videoPerfect})
            </cr-radio-button>
            <cr-radio-button name="0">
              $i18n{na}
            </cr-radio-button>
          </cr-radio-group>
        </div>
        <div class="question-part">
          <div class="h3">$i18n{videoQuality}</div>
          <cr-radio-group .selected="${this.videoQuality_}"
              @selected-changed="${this.onVideoQualityChanged_}">
            <cr-radio-button name="1">
              1 ($i18n{videoUnwatchable})
            </cr-radio-button>
            <cr-radio-button name="2">
              2 ($i18n{videoPoor})
            </cr-radio-button>
            <cr-radio-button name="3">
              3 ($i18n{videoAcceptable})
            </cr-radio-button>
            <cr-radio-button name="4">
              4 ($i18n{videoGood})
            </cr-radio-button>
            <cr-radio-button name="5">
              5 ($i18n{videoGreat})
            </cr-radio-button>
            <cr-radio-button name="0">
              $i18n{na}
            </cr-radio-button>
          </cr-radio-group>
        </div>
        <div class="question-part">
          <div class="h3">$i18n{audioQuality}</div>
          <cr-radio-group .selected="${this.audioQuality_}"
              @selected-changed="${this.onAudioQualityChanged_}">
            <cr-radio-button name="1">
              1 ($i18n{audioUnintelligible})
            </cr-radio-button>
            <cr-radio-button name="2">
              2 ($i18n{audioPoor})
            </cr-radio-button>
            <cr-radio-button name="3">
              3 ($i18n{audioAcceptable})
            </cr-radio-button>
            <cr-radio-button name="4">
              4 ($i18n{audioGood})
            </cr-radio-button>
            <cr-radio-button name="5">
              5 ($i18n{audioPerfect})
            </cr-radio-button>
            <cr-radio-button name="0">
              $i18n{na}
            </cr-radio-button>
          </cr-radio-group>
        </div>
      </div>
      <div class="question">
        <div class="h2">$i18n{contentQuestion}</div>
        <cr-input placeholder="$i18n{yourAnswer}"
            .value="${this.projectedContentUrl_}"
            @value-changed="${this.onProjectedContentUrlChanged_}">
        </cr-input>
      </div>
      <div class="question">
        <div class="h2">
          $i18n{additionalComments}
          <span class="required-message"
              ?hidden="${this.sufficientFeedback_}">*</span>
        </div>
        <textarea placeholder="$i18n{yourAnswer}" rows="8" cols="60"
            .value="${this.comments_}" @input="${this.onCommentsInput_}">
        </textarea>
      </div>
    ` : ''}
    ${this.showDiscoverySection_() ? html`
      <div class="question">
        <div class="h2">
          $i18nRaw{setupVisibilityQuestion}
          <span class="required-message"
              ?hidden="${this.sufficientFeedback_}">*</span>
        </div>
        <cr-radio-group .selected="${this.visibleInSetup_}"
            @selected-changed="${this.onVisibleInSetupChanged_}">
          <cr-radio-button name="Yes">
            $i18n{yes}
          </cr-radio-button>
          <cr-radio-button name="No">
            $i18n{no}
          </cr-radio-button>
          <cr-radio-button value="Unknown">
            $i18n{didNotTry}
          </cr-radio-button>
        </cr-radio-group>
      </div>
      <div class="question">
        <div class="h2">$i18n{softwareQuestion}</div>
        <cr-radio-group .selected="${this.hasNetworkSoftware_}"
            @selected-changed="${this.onHasNetworkSoftwareChanged_}">
          <cr-radio-button name="Yes">
            $i18n{yes}
          </cr-radio-button>
          <cr-radio-button name="No">
            $i18n{no}
          </cr-radio-button>
          <cr-radio-button name="Unknown">
            $i18n{unknown}
          </cr-radio-button>
        </cr-radio-group>
      </div>
      <div class="question">
        <div class="h2">$i18n{networkQuestion}</div>
        <cr-radio-group ng-model="networkDescription">
          <cr-radio-button name="SameWifi">
            $i18n{networkSameWifi}
          </cr-radio-button>
          <cr-radio-button name="DifferentWifi">
            $i18n{networkDifferentWifi}
          </cr-radio-button>
          <cr-radio-button name="WiredPC">
            $i18n{networkWiredPc}
          </cr-radio-button>
        </cr-radio-group>
      </div>
      <div class="question">
        <div class="h2">
          $i18n{additionalComments}
          <span class="required-message"
              ?hidden="${this.sufficientFeedback_}">*</span>
        </div>
        <textarea placeholder="$i18n{yourAnswer}"
            rows="8" cols="60" value="{{comments_::input}}">
        </textarea>
      </div>
    ` : ''}
    <div class="question">
      <!-- Show the email field only if the user is logged into Chrome
           (i.e. |userEmail_| is set). -->
      <cr-checkbox ?checked="${this.allowContactByEmail_}"
           @checked-changed="${this.onAllowContactByEmailChanged_}"
           aria-description="$i18n{allowContactByEmail}"
           id="allow-contact-by-email"
           ?hidden="${!this.userEmail_}">
        <span class="checkbox-label">
          $i18n{allowContactByEmail}
        </span>
      </cr-checkbox>
      <!-- We do not allow the user to edit the email address.
           See b/228865049 for context. -->
      <cr-input placeholder="$i18n{yourEmailAddress}"
          .value="${this.userEmail_}"
          type="text"
          ?hidden="${!this.allowContactByEmail_}"
          disabled>
      </cr-input>
      <cr-checkbox ?checked="${this.attachLogs_}"
          @checked-changed="${this.onAttachLogsChanged_}"
          aria-description="$i18n{sendLogs}">
        <span class="checkbox-label" id="send-logs">
          $i18nRaw{sendLogsHtml}
        </span>
      </cr-checkbox>
      <p class="informative">$i18n{privacyDataUsage}</p>
    </div>
    <div id="form-buttons">
      <cr-button class="cancel-button" @click="${this.onCancel_}">
        $i18n{cancel}
      </cr-button>
      <cr-button class="action-button" @click="${this.onSubmit_}"
          ?disabled="${!this.sufficientFeedback_}">
        $i18n{sendButton}
      </cr-button>
    </div>
  </div>

  <cr-dialog id="sendDialog">
    <div slot="body">${this.sendDialogText_}</div>
    <div slot="button-container">
      <cr-button @click="${this.onSendDialogOk_}"
          ?hidden="${!this.sendDialogIsInteractive_}">
        $i18n{ok}
      </cr-button>
    </div>
  </cr-dialog>

  <cr-dialog id="logsDialog">
    <div slot="title">$i18n{logsHeader}</div>
    <div slot="body">
      <div id="fine-log-warning" class="informative">
        $i18n{fineLogsWarning}
      </div>
      <pre>${this.logData_}</pre>
      <div class="send-logs">
        <cr-checkbox ?checked="${this.attachLogs_}"
            @checked-changed="${this.onAttachLogsChanged_}"
            aria-description="$i18n{sendLogs}">
          <span class="send-logs">$i18n{sendLogs}</span>
        </cr-checkbox>
      </div>
    </div>
    <div slot="button-container">
      <cr-button @click="${this.onLogsDialogOk_}">$i18n{ok}</cr-button>
    </div>
  </cr-dialog>
</div><!--_html_template_end_-->`;
  // clang-format on
}
