// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';

import {HelpContentList} from './feedback_types.js';
import {FeedbackContext, HelpContentType, SearchRequest, SearchResponse} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

export const fakePopularHelpContentList: HelpContentList = [
  {
    title: stringToMojoString16('fake article'),
    url: {url: 'https://support.google.com/chromebook/?q=article'},
    contentType: HelpContentType.kArticle,
  },
  {
    title: stringToMojoString16('fake forum'),
    url: {url: 'https://support.google.com/chromebook/?q=forum'},
    contentType: HelpContentType.kForum,
  },
];

export const fakeHelpContentList: HelpContentList = [
  {
    title: stringToMojoString16('Fix connection problems'),
    url: {url: 'https://support.google.com/chromebook/?q=6318213'},
    contentType: HelpContentType.kArticle,
  },
  {
    title: stringToMojoString16(
        'Why won\'t my wireless mouse with a USB piece wor...?'),
    url: {url: 'https://support.google.com/chromebook/?q=123920509'},
    contentType: HelpContentType.kForum,
  },
  {
    title: stringToMojoString16('Wifi Issues - only on Chromebooks'),
    url: {url: 'https://support.google.com/chromebook/?q=114174470'},
    contentType: HelpContentType.kForum,
  },
  {
    title: stringToMojoString16('Network Connectivity Fault'),
    url: {url: 'https://support.google.com/chromebook/?q=131459420'},
    contentType: HelpContentType.kForum,
  },
  {
    title: stringToMojoString16(
        'Connected to WiFi but can\'t connect to the internet'),
    url: {url: 'https://support.google.com/chromebook/?q=22864239'},
    contentType: HelpContentType.kUnknown,
  },
];

export const fakeEmptyHelpContentList: HelpContentList = [];

export const fakeSearchRequest: SearchRequest = {
  maxResults: 5,
  query: stringToMojoString16('wifi not working'),
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
  assistantDebugInfoAllowed: false,
  autofillMetadata: '',
  categoryTag: 'MediaApp',
  email: 'test.user2@test.com',
  extraDiagnostics: null,
  fromAssistant: false,
  fromAutofill: false,
  fromSettingsSearch: false,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: 'chrome://tab/'},
  traceId: 1,
  wifiDebugLogsAllowed: false,
};

export const fakeEmptyFeedbackContext: FeedbackContext = {
  assistantDebugInfoAllowed: false,
  autofillMetadata: '',
  categoryTag: '',
  email: '',
  extraDiagnostics: null,
  fromAssistant: false,
  fromAutofill: false,
  fromSettingsSearch: false,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: ''},
  traceId: 0,
  wifiDebugLogsAllowed: false,
};

/** Feedback context for login flow, i.e., on oobe or login screen. */
export const fakeLoginFlowFeedbackContext: FeedbackContext = {
  assistantDebugInfoAllowed: false,
  autofillMetadata: '',
  categoryTag: 'Login',
  email: '',
  extraDiagnostics: null,
  fromAssistant: false,
  fromAutofill: false,
  fromSettingsSearch: false,
  hasLinkedCrossDevicePhone: false,
  isInternalAccount: false,
  pageUrl: {url: ''},
  traceId: 0,
  wifiDebugLogsAllowed: false,
};

export const fakeInternalUserFeedbackContext: FeedbackContext = {
  assistantDebugInfoAllowed: false,
  autofillMetadata: '',
  categoryTag: '',
  email: 'test.user@google.com',
  extraDiagnostics: null,
  fromAssistant: true,
  fromAutofill: false,
  fromSettingsSearch: true,
  hasLinkedCrossDevicePhone: true,
  isInternalAccount: true,
  pageUrl: {url: 'chrome://tab/'},
  traceId: 1,
  wifiDebugLogsAllowed: false,
};

export const fakeFeedbackContextWithoutLinkedCrossDevicePhone:
    FeedbackContext = {
      assistantDebugInfoAllowed: false,
      autofillMetadata: '',
      categoryTag: '',
      email: 'test.user@google.com',
      extraDiagnostics: null,
      fromAssistant: true,
      fromAutofill: false,
      fromSettingsSearch: true,
      hasLinkedCrossDevicePhone: false,
      isInternalAccount: true,
      pageUrl: {url: 'chrome://tab/'},
      traceId: 1,
      wifiDebugLogsAllowed: false,
    };

export const fakeFeedbackContextWithExtraDiagnostics: FeedbackContext = {
  assistantDebugInfoAllowed: false,
  autofillMetadata: '',
  categoryTag: '',
  email: 'test.user@google.com',
  extraDiagnostics: 'some extra info',
  fromAssistant: true,
  fromAutofill: false,
  fromSettingsSearch: true,
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
