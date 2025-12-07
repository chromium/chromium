// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HelpContentList} from './feedback_types.js';
import type {FeedbackContext, SearchRequest, SearchResponse} from './os_feedback_ui.mojom-webui.js';
import {HelpContentType} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

export const fakePopularHelpContentList: HelpContentList = [
  {
    title: 'fake article',
    url: {url: 'https://support.google.com/chromebook/?q=article'},
    contentType: HelpContentType.kArticle,
  },
  {
    title: 'fake forum',
    url: {url: 'https://support.google.com/chromebook/?q=forum'},
    contentType: HelpContentType.kForum,
  },
];

export const fakeHelpContentList: HelpContentList = [
  {
    title: 'Fix connection problems',
    url: {url: 'https://support.google.com/chromebook/?q=6318213'},
    contentType: HelpContentType.kArticle,
  },
  {
    title: 'Why won\'t my wireless mouse with a USB piece wor...?',
    url: {url: 'https://support.google.com/chromebook/?q=123920509'},
    contentType: HelpContentType.kForum,
  },
  {
    title: 'Wifi Issues - only on Chromebooks',
    url: {url: 'https://support.google.com/chromebook/?q=114174470'},
    contentType: HelpContentType.kForum,
  },
  {
    title: 'Network Connectivity Fault',
    url: {url: 'https://support.google.com/chromebook/?q=131459420'},
    contentType: HelpContentType.kForum,
  },
  {
    title: 'Connected to WiFi but can\'t connect to the internet',
    url: {url: 'https://support.google.com/chromebook/?q=22864239'},
    contentType: HelpContentType.kUnknown,
  },
];

export const fakeEmptyHelpContentList: HelpContentList = [];

export const fakeSearchRequest: SearchRequest = {
  maxResults: 5,
  query: 'wifi not working',
};

export const fakeSearchResponse: SearchResponse = {
  results: fakeHelpContentList,
  totalResults: 10,
};

export const fakeEmptySearchResponse: SearchResponse = {
  results: fakeEmptyHelpContentList,
  totalResults: 0,
};

export const fakeFeedbackContext: FeedbackContext = {
  autofillMetadata: '',
  categoryTag: 'MediaApp',
  email: 'test.user2@test.com',
  extraDiagnostics: null,
  fromAutofill: false,
  settingsSearchDoNotRecordMetrics: true,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: 'chrome://tab/'},
  traceId: 1,
  wifiDebugLogsAllowed: false,
};

export const fakeEmptyFeedbackContext: FeedbackContext = {
  autofillMetadata: '',
  categoryTag: '',
  email: '',
  extraDiagnostics: null,
  fromAutofill: false,
  settingsSearchDoNotRecordMetrics: true,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: ''},
  traceId: 0,
  wifiDebugLogsAllowed: false,
};

/** Feedback context for login flow, i.e., on oobe or login screen. */
export const fakeLoginFlowFeedbackContext: FeedbackContext = {
  autofillMetadata: '',
  categoryTag: 'Login',
  email: '',
  extraDiagnostics: null,
  fromAutofill: false,
  settingsSearchDoNotRecordMetrics: true,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: ''},
  traceId: 0,
  wifiDebugLogsAllowed: false,
};

export const fakeInternalUserFeedbackContext: FeedbackContext = {
  autofillMetadata: '',
  categoryTag: '',
  email: 'test.user@google.com',
  extraDiagnostics: null,
  fromAutofill: false,
  settingsSearchDoNotRecordMetrics: false,
  hasLinkedCrossDevicePhone: true,
  isInternalAccount: true,
  pageUrl: {url: 'chrome://tab/'},
  traceId: 1,
  wifiDebugLogsAllowed: false,
};

export const fakeFeedbackContextWithoutLinkedCrossDevicePhone:
    FeedbackContext = {
      autofillMetadata: '',
      categoryTag: '',
      email: 'test.user@google.com',
      extraDiagnostics: null,
      fromAutofill: false,
      settingsSearchDoNotRecordMetrics: false,
      hasLinkedCrossDevicePhone: false,
      isInternalAccount: true,
      pageUrl: {url: 'chrome://tab/'},
      traceId: 1,
      wifiDebugLogsAllowed: false,
    };

export const fakeFeedbackContextWithExtraDiagnostics: FeedbackContext = {
  autofillMetadata: '',
  categoryTag: '',
  email: 'test.user@google.com',
  extraDiagnostics: 'some extra info',
  fromAutofill: false,
  settingsSearchDoNotRecordMetrics: false,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: true,
  pageUrl: {url: 'chrome://tab/'},
  traceId: 1,
  wifiDebugLogsAllowed: false,
};
export const fakePngData: number[] = [
  137, 80,  78, 71,  13,  10, 26, 10,  0,  0,  0,   13,  73,  72,  68,  82,
  0,   0,   0,  8,   0,   0,  0,  8,   8,  2,  0,   0,   0,   75,  109, 41,
  220, 0,   0,  0,   34,  73, 68, 65,  84, 8,  215, 99,  120, 173, 168, 135,
  21,  49,  0,  241, 255, 15, 90, 104, 8,  33, 129, 83,  7,   97,  163, 136,
  214, 129, 93, 2,   43,  2,  0,  181, 31, 90, 179, 225, 252, 176, 37,  0,
  0,   0,   0,  73,  69,  78, 68, 174, 66, 96, 130,
];
