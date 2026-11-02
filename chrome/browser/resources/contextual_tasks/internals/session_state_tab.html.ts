// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksSessionStateTabElement} from './session_state_tab.js';

export function getHtml(this: ContextualTasksSessionStateTabElement) {
  // clang-format off
  return html`
<div class="card">
  <div class="session-state-header">
    <div>
      <h2>Tracked WebContents &amp; Session Handles</h2>
      <p class="section-subtitle">
        Inspects open tabs and cached side panels with an attached
        ContextualTasksWebContentsUserData or ContextualSearchSessionHandle.
      </p>
    </div>
    <cr-button id="refreshSessionStateButton" class="action-button"
        @click="${this.onRefreshSessionStateClick_}">
      Refresh
    </cr-button>
  </div>
</div>
${this.webContentsDebugStates_.length === 0 ? html`
  <div class="card empty-state-card">
    <p class="empty-state-text">
      No open tabs or cached side panels currently have an attached
      ContextualTasksWebContentsUserData or ContextualSearchSessionHandle.
    </p>
  </div>
` : ''}
${this.webContentsDebugStates_.map(wc => html`
  <div class="card wc-debug-card">
    <div class="wc-card-header">
      <div class="wc-title-row">
        <span class="badge ${
            wc.isSidePanel ? 'badge-side-panel' : 'badge-tab'}">
          ${wc.isSidePanel ? 'Side Panel' : 'Browser Tab'}
        </span>
        ${wc.isActiveTabInWindow ? html`
          <span class="badge badge-ok">Active Tab</span>
        ` : ''}
        <span class="wc-title">${wc.title || '(Untitled)'}</span>
      </div>
      <div class="wc-meta-row">
        <span class="mono-pill">
          SessionID: ${wc.sessionTabId >= 0 ? wc.sessionTabId : 'N/A'}
        </span>
        <span class="mono-pill">
          TabHandle: ${wc.tabHandleId >= 0 ? wc.tabHandleId : 'N/A'}
        </span>
        <span class="wc-url">${wc.url || 'about:blank'}</span>
      </div>
    </div>

    <div class="debug-subsection">
      <h3 class="subsection-title">a) User Data Model State</h3>
      ${!wc.userDataState ? html`
        <p class="empty-state-text">No user data state attached.</p>
      ` : html`
        <div class="kv-grid">
          <div class="kv-group">
            <h4>Task &amp; Tab Identity</h4>
            <div class="kv-item">
              <span class="kv-label">ContextualTasksUserData</span>
              <span class="badge ${
                  wc.userDataState.hasContextualTasksUserData ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.userDataState.hasContextualTasksUserData ?
                    'Attached' :
                    'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">ContextualSearchHelper</span>
              <span class="badge ${
                  wc.userDataState.hasContextualSearchHelper ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.userDataState.hasContextualSearchHelper ?
                    'Attached' :
                    'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">user_data.task_id</span>
              <span class="mono-value">
                ${wc.userDataState.userDataTaskId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">pending_task_id</span>
              <span class="mono-value">
                ${wc.userDataState.pendingTaskId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">GetEffectiveTaskId()</span>
              <span class="mono-value">
                ${wc.userDataState.effectiveTaskId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">helper.task_id</span>
              <span class="mono-value">
                ${wc.userDataState.helperTaskId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Service Task for Tab</span>
              <span class="mono-value">
                ${wc.userDataState.serviceTaskIdForTab || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Helper Selected Tab IDs</span>
              <span class="mono-value">
                ${wc.userDataState.helperSelectedTabIds.length > 0 ?
                    wc.userDataState.helperSelectedTabIds.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Active Underline Tab IDs</span>
              <span class="mono-value">
                ${wc.userDataState.activeUnderlineTabIds.length > 0 ?
                    wc.userDataState.activeUnderlineTabIds.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">TabHandle &harr; SessionID</span>
              <span class="mono-value">
                ${wc.userDataState.tabIdMappings.length > 0 ?
                    wc.userDataState.tabIdMappings
                        .map(m => `${m.tabHandleId}\u2192${m.sessionTabId}`)
                        .join(', ') :
                    'None'}
              </span>
            </div>
          </div>

          <div class="kv-group">
            <h4>PageSearchState &amp; IPC</h4>
            <div class="kv-item">
              <span class="kv-label">connected_document_id</span>
              <span class="mono-value">
                ${wc.userDataState.connectedDocumentId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">SW Port Connected</span>
              <span class="badge ${
                  wc.userDataState.isServiceWorkerPortConnected ?
                      'badge-ok' :
                      'badge-warn'}">
                ${wc.userDataState.isServiceWorkerPortConnected ?
                    'Yes' :
                    'No'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">is_handshake_complete</span>
              <span class="badge ${
                  wc.userDataState.isHandshakeComplete ?
                      'badge-ok' :
                      'badge-warn'}">
                ${wc.userDataState.isHandshakeComplete ?
                    'Complete' :
                    'Pending'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">pending_search_messages</span>
              <span class="mono-value">
                ${wc.userDataState.pendingSearchMessagesCount}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">context_library_is_active</span>
              <span class="badge ${
                  wc.userDataState.contextLibraryIsActive ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.userDataState.contextLibraryIsActive ?
                    'Active' :
                    'Inactive'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">is_lens_crop_mounted</span>
              <span class="badge ${
                  wc.userDataState.isLensCropMounted ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.userDataState.isLensCropMounted ? 'Mounted' : 'No'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">last_sent_context_state</span>
              <span class="mono-pill">
                ${wc.userDataState.lastSentContextState}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">has_bound_extension_frame</span>
              <span class="badge ${
                  wc.userDataState.hasBoundExtensionFrame ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.userDataState.hasBoundExtensionFrame ? 'Yes' : 'No'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">
                Extension Frames
                (${wc.userDataState.extensionFrames.length})
              </span>
              <span class="mono-value">
                ${wc.userDataState.extensionFrames.length > 0 ?
                    wc.userDataState.extensionFrames
                        .map(
                            f => `#${f.index}[bound=${
                                String(f.isPageBound)}, hs=${
                                String(f.isHandshakeComplete)}]`)
                        .join(', ') :
                    'None'}
              </span>
            </div>
          </div>

          <div class="kv-group">
            <h4>Upload &amp; Submit Tracking</h4>
            <div class="kv-item">
              <span class="kv-label">submit_pending_</span>
              <span class="badge ${
                  wc.userDataState.submitPending ?
                      'badge-warn' :
                      'badge-neutral'}">
                ${wc.userDataState.submitPending ? 'Pending' : 'False'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">pending_context_uploads_</span>
              <span class="mono-value">
                ${wc.userDataState.pendingContextUploadTokens.length > 0 ?
                    wc.userDataState.pendingContextUploadTokens.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">failed_context_uploads_</span>
              <span class="mono-value">
                ${wc.userDataState.failedContextUploadTokens.length > 0 ?
                    wc.userDataState.failedContextUploadTokens.join(', ') :
                    '[]'}
              </span>
            </div>
          </div>

          <div class="kv-group">
            <h4>InputStateModel</h4>
            ${!wc.userDataState.inputState ? html`
              <p class="empty-state-text">
                No InputStateModel instantiated.
              </p>
            ` : html`
              <div class="kv-item">
                <span class="kv-label">has_valid_config</span>
                <span class="mono-value">
                  ${wc.userDataState.inputState.hasValidConfig ?
                      'true' :
                      'false'}
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">Smart Tab Sharing / Lens Crop</span>
                <span class="mono-value">
                  ${wc.userDataState.inputState.isSmartTabSharingActive ?
                      'true' :
                      'false'} /
                  ${wc.userDataState.inputState.hasLensCrop ?
                      'true' :
                      'false'}
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">active_tool / active_model</span>
                <span class="mono-value">
                  ${wc.userDataState.inputState.activeTool} /
                  ${wc.userDataState.inputState.activeModel}
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">Allowed / Disabled Tools</span>
                <span class="mono-value">
                  [${wc.userDataState.inputState.allowedTools.join(', ')}] /
                  [${wc.userDataState.inputState.disabledTools.join(', ')}]
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">Allowed / Disabled Models</span>
                <span class="mono-value">
                  [${wc.userDataState.inputState.allowedModels.join(', ')}]
                  /
                  [${wc.userDataState.inputState.disabledModels.join(', ')}]
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">Allowed / Disabled Input Types</span>
                <span class="mono-value">
                  [${wc.userDataState.inputState.allowedInputTypes
                        .join(', ')}]
                  /
                  [${wc.userDataState.inputState.disabledInputTypes
                        .join(', ')}]
                </span>
              </div>
              <div class="kv-item">
                <span class="kv-label">Effective Input Types</span>
                <span class="mono-value">
                  [${wc.userDataState.inputState.effectiveInputTypes
                        .join(', ')}]
                </span>
              </div>
            `}
          </div>
        </div>

        ${wc.userDataState.selectedTabs.length > 0 ? html`
          <h4 class="table-heading">
            UserData Selected Tabs (${wc.userDataState.selectedTabs.length})
          </h4>
          <table>
            <thead>
              <tr>
                <th>Tab ID</th>
                <th>Title</th>
                <th>Context Token</th>
                <th>Request ID</th>
                <th>Flags</th>
              </tr>
            </thead>
            <tbody>
              ${wc.userDataState.selectedTabs.map(tab => html`
                <tr>
                  <td>${tab.tabId}</td>
                  <td title="${tab.url}">${tab.title || tab.url}</td>
                  <td>${tab.contextToken}</td>
                  <td>${tab.requestId || 'None'}</td>
                  <td>
                    ${[
                      tab.uploaded && 'Uploaded',
                      tab.submitted && 'Submitted',
                      tab.pendingDelayedUpload && 'Delayed',
                      tab.restoredFromAim && 'Restored',
                    ].filter(Boolean).join(' ') || 'None'}
                  </td>
                </tr>
              `)}
            </tbody>
          </table>
        ` : ''}
      `}
    </div>

    <div class="debug-subsection">
      <h3 class="subsection-title">b) Session Handle Data</h3>
      ${!wc.sessionHandleState ? html`
        <p class="empty-state-text">
          No ContextualSearchSessionHandle attached.
        </p>
      ` : html`
        <div class="kv-grid">
          <div class="kv-group">
            <h4>Session Metadata</h4>
            <div class="kv-item">
              <span class="kv-label">session_id</span>
              <span class="mono-value">
                ${wc.sessionHandleState.sessionId}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">search_session_id</span>
              <span class="mono-value">
                ${wc.sessionHandleState.searchSessionId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">current_url</span>
              <span class="mono-value">
                ${wc.sessionHandleState.currentUrl || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">invocation_source</span>
              <span class="mono-pill">
                ${wc.sessionHandleState.invocationSource}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">is_contextual_lens_session</span>
              <span class="badge ${
                  wc.sessionHandleState.isContextualLensSession ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.sessionHandleState.isContextualLensSession ?
                    'Yes' :
                    'No'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">auth_user_index</span>
              <span class="mono-value">
                ${wc.sessionHandleState.authUserIndex}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">has_submitted_context</span>
              <span class="badge ${
                  wc.sessionHandleState.hasSubmittedContext ?
                      'badge-ok' :
                      'badge-neutral'}">
                ${wc.sessionHandleState.hasSubmittedContext ? 'Yes' : 'No'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Smart Tab Sharing / Toggled</span>
              <span class="mono-value">
                ${wc.sessionHandleState.smartTabSharingActive ?
                    'true' :
                    'false'} /
                ${wc.sessionHandleState
                      .smartTabSharingToggledSinceLastTurn ?
                    'true' :
                    'false'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">STS Toggled Removed IDs</span>
              <span class="mono-value">
                ${wc.sessionHandleState.stsToggledRemovedContextIds.length >
                        0 ?
                    wc.sessionHandleState.stsToggledRemovedContextIds
                        .join(', ') :
                    '[]'}
              </span>
            </div>
          </div>

          <div class="kv-group">
            <h4>Context Tokens &amp; Persisted State</h4>
            <div class="kv-item">
              <span class="kv-label">Uploaded Context Tokens</span>
              <span class="mono-value">
                ${wc.sessionHandleState.uploadedContextTokens.length > 0 ?
                    wc.sessionHandleState.uploadedContextTokens.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Submitted Context Tokens</span>
              <span class="mono-value">
                ${wc.sessionHandleState.submittedContextTokens.length > 0 ?
                    wc.sessionHandleState.submittedContextTokens
                        .join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Active Submitted Tokens</span>
              <span class="mono-value">
                ${wc.sessionHandleState.activeSubmittedContextTokens
                            .length > 0 ?
                    wc.sessionHandleState.activeSubmittedContextTokens
                        .join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Persisted Tab IDs</span>
              <span class="mono-value">
                ${wc.sessionHandleState.persistedTabIds.length > 0 ?
                    wc.sessionHandleState.persistedTabIds.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Pending Delayed Tab IDs</span>
              <span class="mono-value">
                ${wc.sessionHandleState.pendingDelayedTabIds.length > 0 ?
                    wc.sessionHandleState.pendingDelayedTabIds.join(', ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Deselected Tabs</span>
              <span class="mono-value">
                ${wc.sessionHandleState.deselectedTabs.length > 0 ?
                    wc.sessionHandleState.deselectedTabs
                        .map(t => `#${t.tabId} (${t.title || t.url})`)
                        .join(', ') :
                    '[]'}
              </span>
            </div>
          </div>

          <div class="kv-group">
            <h4>Task Thread &amp; Turn History</h4>
            <div class="kv-item">
              <span class="kv-label">Thread Type / Server ID</span>
              <span class="mono-value">
                ${wc.sessionHandleState.threadType || 'None'} /
                ${wc.sessionHandleState.threadServerId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Thread Title</span>
              <span class="mono-value">
                ${wc.sessionHandleState.threadTitle || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">Conversation Turn ID</span>
              <span class="mono-value">
                ${wc.sessionHandleState.threadConversationTurnId || 'None'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">
                Previous Turns
                (${wc.sessionHandleState.previousTurns.length})
              </span>
              <span class="mono-value">
                ${wc.sessionHandleState.previousTurns.length > 0 ?
                    wc.sessionHandleState.previousTurns.join(' | ') :
                    '[]'}
              </span>
            </div>
            <div class="kv-item">
              <span class="kv-label">
                Task URL Resources
                (${wc.sessionHandleState.urlResources.length})
              </span>
              <span class="mono-value">
                ${wc.sessionHandleState.urlResources.length > 0 ?
                    wc.sessionHandleState.urlResources
                        .map(
                            r => `[${r.resourceType}] tab=${r.tabId} ctx=${
                                r.contextId} ${r.title || r.url}`)
                        .join('; ') :
                    '[]'}
              </span>
            </div>
          </div>
        </div>

        ${wc.sessionHandleState.attachedTabs.length > 0 ||
          wc.sessionHandleState.restoredTabs.length > 0 ? html`
          <h4 class="table-heading">
            TabContextState (Attached:
            ${wc.sessionHandleState.attachedTabs.length},
            Restored: ${wc.sessionHandleState.restoredTabs.length})
          </h4>
          <table>
            <thead>
              <tr>
                <th>Bucket</th>
                <th>Tab ID</th>
                <th>Title / URL</th>
                <th>Context Token</th>
                <th>Request ID</th>
                <th>Flags</th>
              </tr>
            </thead>
            <tbody>
              ${wc.sessionHandleState.attachedTabs.map(tab => html`
                <tr>
                  <td>Attached</td>
                  <td>${tab.tabId}</td>
                  <td title="${tab.url}">${tab.title || tab.url}</td>
                  <td>${tab.contextToken}</td>
                  <td>${tab.requestId || 'None'}</td>
                  <td>
                    ${[
                      tab.uploaded && 'Uploaded',
                      tab.submitted && 'Submitted',
                      tab.pendingDelayedUpload && 'Delayed',
                      tab.restoredFromAim && 'Restored',
                    ].filter(Boolean).join(' ') || 'None'}
                  </td>
                </tr>
              `)}
              ${wc.sessionHandleState.restoredTabs.map(tab => html`
                <tr>
                  <td>Restored</td>
                  <td>${tab.tabId}</td>
                  <td title="${tab.url}">${tab.title || tab.url}</td>
                  <td>${tab.contextToken}</td>
                  <td>${tab.requestId || 'None'}</td>
                  <td>
                    ${[
                      tab.uploaded && 'Uploaded',
                      tab.submitted && 'Submitted',
                      tab.pendingDelayedUpload && 'Delayed',
                      tab.restoredFromAim && 'Restored',
                    ].filter(Boolean).join(' ') || 'None'}
                  </td>
                </tr>
              `)}
            </tbody>
          </table>
        ` : ''}

        ${wc.sessionHandleState.uploadedFiles.length > 0 ||
          wc.sessionHandleState.submittedFiles.length > 0 ? html`
          <h4 class="table-heading">
            Context FileInfos (Uploaded:
            ${wc.sessionHandleState.uploadedFiles.length},
            Submitted: ${wc.sessionHandleState.submittedFiles.length})
          </h4>
          <table>
            <thead>
              <tr>
                <th>Set</th>
                <th>Token</th>
                <th>Name / Tab</th>
                <th>MIME / Size</th>
                <th>Status / Error</th>
                <th>Request ID / Flags</th>
              </tr>
            </thead>
            <tbody>
              ${wc.sessionHandleState.uploadedFiles.map(f => html`
                <tr>
                  <td>Uploaded</td>
                  <td>${f.fileToken}</td>
                  <td title="${f.tabUrl}">
                    ${f.fileName || f.tabTitle || f.tabUrl}
                    (tab: ${f.tabSessionId})
                  </td>
                  <td>${f.mimeType} (${f.fileSizeBytes}B)</td>
                  <td>${f.uploadStatus} / ${f.uploadErrorType}</td>
                  <td>
                    ${f.requestId || 'None'}
                    ${f.isImplicitUpload ? '[Implicit] ' : ''}
                    ${f.isSuperceded ? '[Superceded]' : ''}
                  </td>
                </tr>
              `)}
              ${wc.sessionHandleState.submittedFiles.map(f => html`
                <tr>
                  <td>Submitted</td>
                  <td>${f.fileToken}</td>
                  <td title="${f.tabUrl}">
                    ${f.fileName || f.tabTitle || f.tabUrl}
                    (tab: ${f.tabSessionId})
                  </td>
                  <td>${f.mimeType} (${f.fileSizeBytes}B)</td>
                  <td>${f.uploadStatus} / ${f.uploadErrorType}</td>
                  <td>
                    ${f.requestId || 'None'}
                    ${f.isImplicitUpload ? '[Implicit] ' : ''}
                    ${f.isSuperceded ? '[Superceded]' : ''}
                  </td>
                </tr>
              `)}
            </tbody>
          </table>
        ` : ''}
      `}
    </div>
  </div>
`)}
`;
  // clang-format on
}
