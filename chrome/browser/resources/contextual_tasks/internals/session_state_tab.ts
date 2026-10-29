// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './session_state_tab.css.js';
import {getHtml} from './session_state_tab.html.js';

export interface TabIdMappingDebug {
  tabHandleId: number;
  sessionTabId: number;
}

export interface ExtensionFrameDebug {
  index: number;
  isPageBound: boolean;
  isHandshakeComplete: boolean;
}

export interface TabContextEntryDebug {
  tabId: number;
  title: string;
  url: string;
  contextToken: string;
  requestId: string;
  uploaded: boolean;
  submitted: boolean;
  pendingDelayedUpload: boolean;
  restoredFromAim: boolean;
}

export interface InputStateDebugState {
  hasValidConfig: boolean;
  isSmartTabSharingActive: boolean;
  hasLensCrop: boolean;
  activeTool: string;
  activeModel: string;
  allowedTools: string[];
  disabledTools: string[];
  allowedModels: string[];
  disabledModels: string[];
  allowedInputTypes: string[];
  disabledInputTypes: string[];
  effectiveInputTypes: string[];
}

export interface UserDataDebugState {
  hasContextualTasksUserData: boolean;
  hasContextualSearchHelper: boolean;
  userDataTaskId: string;
  pendingTaskId: string;
  effectiveTaskId: string;
  helperTaskId: string;
  serviceTaskIdForTab: string;
  helperSelectedTabIds: number[];
  activeUnderlineTabIds: number[];
  tabIdMappings: TabIdMappingDebug[];
  connectedDocumentId: string;
  isServiceWorkerPortConnected: boolean;
  isHandshakeComplete: boolean;
  pendingSearchMessagesCount: number;
  contextLibraryIsActive: boolean;
  isLensCropMounted: boolean;
  lastSentContextState: string;
  hasBoundExtensionFrame: boolean;
  extensionFrames: ExtensionFrameDebug[];
  submitPending: boolean;
  pendingContextUploadTokens: string[];
  failedContextUploadTokens: string[];
  selectedTabs: TabContextEntryDebug[];
  inputState: InputStateDebugState|null;
}

export interface DeselectedTabDebug {
  tabId: number;
  title: string;
  url: string;
}

export interface FileInfoDebug {
  fileToken: string;
  fileName: string;
  fileSizeBytes: bigint;
  mimeType: string;
  uploadStatus: string;
  uploadErrorType: string;
  requestId: string;
  tabSessionId: number;
  tabUrl: string;
  tabTitle: string;
  isSuperceded: boolean;
  isImplicitUpload: boolean;
}

export interface UrlResourceDebug {
  urlId: string;
  url: string;
  tabId: number;
  title: string;
  contextId: bigint;
  resourceType: string;
}

export interface SessionHandleDebugState {
  sessionId: string;
  searchSessionId: string;
  currentUrl: string;
  invocationSource: string;
  isContextualLensSession: boolean;
  authUserIndex: number;
  hasSubmittedContext: boolean;
  smartTabSharingActive: boolean;
  smartTabSharingToggledSinceLastTurn: boolean;
  stsToggledRemovedContextIds: string[];
  attachedTabs: TabContextEntryDebug[];
  restoredTabs: TabContextEntryDebug[];
  deselectedTabs: DeselectedTabDebug[];
  uploadedContextTokens: string[];
  submittedContextTokens: string[];
  activeSubmittedContextTokens: string[];
  persistedTabIds: number[];
  pendingDelayedTabIds: number[];
  uploadedFiles: FileInfoDebug[];
  submittedFiles: FileInfoDebug[];
  previousTurns: string[];
  threadType: string;
  threadServerId: string;
  threadTitle: string;
  threadConversationTurnId: string;
  urlResources: UrlResourceDebug[];
}

export interface WebContentsDebugState {
  isSidePanel: boolean;
  title: string;
  url: string;
  sessionTabId: number;
  tabHandleId: number;
  isActiveTabInWindow: boolean;
  userDataState: UserDataDebugState|null;
  sessionHandleState: SessionHandleDebugState|null;
}

const MOCK_WEB_CONTENTS_DEBUG_STATES: WebContentsDebugState[] = [
  {
    isSidePanel: true,
    title: 'Google Search - AI Mode (Side Panel)',
    url: 'https://www.google.com/search?udm=50&q=compare+btrfs+vs+ext4',
    sessionTabId: 14,
    tabHandleId: -1,
    isActiveTabInWindow: false,
    userDataState: {
      hasContextualTasksUserData: true,
      hasContextualSearchHelper: true,
      userDataTaskId: '7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90',
      pendingTaskId: '',
      effectiveTaskId: '7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90',
      helperTaskId: '7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90',
      serviceTaskIdForTab: '7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90',
      helperSelectedTabIds: [101, 102],
      activeUnderlineTabIds: [101, 102],
      tabIdMappings: [
        {tabHandleId: 21, sessionTabId: 101},
        {tabHandleId: 22, sessionTabId: 102},
      ],
      connectedDocumentId: '0195A8F0C2E47B109D33E84A11B09C42',
      isServiceWorkerPortConnected: true,
      isHandshakeComplete: true,
      pendingSearchMessagesCount: 0,
      contextLibraryIsActive: true,
      isLensCropMounted: false,
      lastSentContextState: 'kReady',
      hasBoundExtensionFrame: true,
      extensionFrames: [
        {index: 0, isPageBound: true, isHandshakeComplete: true},
      ],
      submitPending: false,
      pendingContextUploadTokens: [],
      failedContextUploadTokens: [],
      selectedTabs: [
        {
          tabId: 101,
          title: 'Btrfs Wiki - Kernel Newbies',
          url: 'https://btrfs.readthedocs.io/en/latest/',
          contextToken: '8A4F19C200E14B7290D123456789ABCD',
          requestId: 'vsrid_1001_seq_1',
          uploaded: true,
          submitted: true,
          pendingDelayedUpload: false,
          restoredFromAim: false,
        },
        {
          tabId: 102,
          title: 'ext4 Data Structures and Algorithms',
          url: 'https://docs.kernel.org/filesystems/ext4/',
          contextToken: '3B7E82D199F04A1188C098765432DCBA',
          requestId: 'vsrid_1002_seq_1',
          uploaded: true,
          submitted: false,
          pendingDelayedUpload: false,
          restoredFromAim: false,
        },
      ],
      inputState: {
        hasValidConfig: true,
        isSmartTabSharingActive: false,
        hasLensCrop: false,
        activeTool: 'TOOL_MODE_UNSPECIFIED',
        activeModel: 'MODEL_MODE_GEMINI_PRO',
        allowedTools: ['TOOL_MODE_DEEP_SEARCH', 'TOOL_MODE_CANVAS'],
        disabledTools: [],
        allowedModels: ['MODEL_MODE_GEMINI_PRO'],
        disabledModels: [],
        allowedInputTypes: [
          'INPUT_TYPE_LENS_IMAGE',
          'INPUT_TYPE_LENS_FILE',
          'INPUT_TYPE_BROWSER_TAB',
        ],
        disabledInputTypes: [],
        effectiveInputTypes: [
          'INPUT_TYPE_LENS_IMAGE',
          'INPUT_TYPE_LENS_FILE',
          'INPUT_TYPE_BROWSER_TAB',
        ],
      },
    },
    sessionHandleState: {
      sessionId: 'A1B2C3D4E5F60718293A4B5C6D7E8F90',
      searchSessionId: 'gsession_9876543210abcdef',
      currentUrl:
          'https://www.google.com/search?udm=50&q=compare+btrfs+vs+ext4',
      invocationSource: 'kContextualTasksComposebox',
      isContextualLensSession: false,
      authUserIndex: 0,
      hasSubmittedContext: true,
      smartTabSharingActive: false,
      smartTabSharingToggledSinceLastTurn: false,
      stsToggledRemovedContextIds: [],
      attachedTabs: [
        {
          tabId: 101,
          title: 'Btrfs Wiki - Kernel Newbies',
          url: 'https://btrfs.readthedocs.io/en/latest/',
          contextToken: '8A4F19C200E14B7290D123456789ABCD',
          requestId: 'vsrid_1001_seq_1',
          uploaded: true,
          submitted: true,
          pendingDelayedUpload: false,
          restoredFromAim: false,
        },
        {
          tabId: 102,
          title: 'ext4 Data Structures and Algorithms',
          url: 'https://docs.kernel.org/filesystems/ext4/',
          contextToken: '3B7E82D199F04A1188C098765432DCBA',
          requestId: 'vsrid_1002_seq_1',
          uploaded: true,
          submitted: false,
          pendingDelayedUpload: false,
          restoredFromAim: false,
        },
      ],
      restoredTabs: [],
      deselectedTabs: [
        {
          tabId: 103,
          title: 'XFS Filesystem Overview',
          url: 'https://xfs.org/',
        },
      ],
      uploadedContextTokens: ['3B7E82D199F04A1188C098765432DCBA'],
      submittedContextTokens: ['8A4F19C200E14B7290D123456789ABCD'],
      activeSubmittedContextTokens: ['8A4F19C200E14B7290D123456789ABCD'],
      persistedTabIds: [101],
      pendingDelayedTabIds: [],
      uploadedFiles: [
        {
          fileToken: '3B7E82D199F04A1188C098765432DCBA',
          fileName: 'ext4 Data Structures and Algorithms',
          fileSizeBytes: 48210n,
          mimeType: 'kAnnotatedPageContent',
          uploadStatus: 'kUploadSuccessful',
          uploadErrorType: 'None',
          requestId: 'vsrid_1002_seq_1',
          tabSessionId: 102,
          tabUrl: 'https://docs.kernel.org/filesystems/ext4/',
          tabTitle: 'ext4 Data Structures and Algorithms',
          isSuperceded: false,
          isImplicitUpload: false,
        },
      ],
      submittedFiles: [
        {
          fileToken: '8A4F19C200E14B7290D123456789ABCD',
          fileName: 'Btrfs Wiki - Kernel Newbies',
          fileSizeBytes: 64120n,
          mimeType: 'kAnnotatedPageContent',
          uploadStatus: 'kUploadSuccessful',
          uploadErrorType: 'None',
          requestId: 'vsrid_1001_seq_1',
          tabSessionId: 101,
          tabUrl: 'https://btrfs.readthedocs.io/en/latest/',
          tabTitle: 'Btrfs Wiki - Kernel Newbies',
          isSuperceded: false,
          isImplicitUpload: false,
        },
      ],
      previousTurns: ['compare btrfs vs ext4'],
      threadType: 'kAiMode',
      threadServerId: 'thread_srv_482910',
      threadTitle: 'Compare Btrfs vs ext4',
      threadConversationTurnId: 'turn_001',
      urlResources: [
        {
          urlId: '9f10b220-1122-4333-8444-556677889900',
          url: 'https://btrfs.readthedocs.io/en/latest/',
          tabId: 101,
          title: 'Btrfs Wiki - Kernel Newbies',
          contextId: 9001001n,
          resourceType: 'kWebpage',
        },
      ],
    },
  },
];

export class ContextualTasksSessionStateTabElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-session-state-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webContentsDebugStates_: {type: Array},
    };
  }

  protected accessor webContentsDebugStates_: WebContentsDebugState[] =
      MOCK_WEB_CONTENTS_DEBUG_STATES;

  protected onRefreshSessionStateClick_() {
    this.webContentsDebugStates_ = [...MOCK_WEB_CONTENTS_DEBUG_STATES];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-session-state-tab': ContextualTasksSessionStateTabElement;
  }
}

customElements.define(
    ContextualTasksSessionStateTabElement.is,
    ContextualTasksSessionStateTabElement);
