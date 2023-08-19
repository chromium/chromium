// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';

import {FeedbackContext, HelpContentList, HelpContentType, SearchRequest, SearchResponse} from './feedback_types.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

/** @type {!HelpContentList} */
export const fakePopularHelpContentList = [
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

/** @type {!HelpContentList} */
export const fakeHelpContentList = [
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

/** @type {!HelpContentList} */
export const fakeEmptyHelpContentList = [];

/** @type {!SearchRequest} */
export const fakeSearchRequest = {
  maxResults: 5,
  query: stringToMojoString16('wifi not working'),
};

/** @type {!SearchResponse} */
export const fakeSearchResponse = {
  results: fakeHelpContentList,
  totalResults: 10,
};

/** @type {!SearchResponse} */
export const fakeEmptySearchResponse = {
  results: fakeEmptyHelpContentList,
  totalResults: 0,
};

/** @type {!FeedbackContext} */
export const fakeFeedbackContext = {
  email: 'test.user2@test.com',
  pageUrl: {url: 'chrome://tab/'},
  isInternalAccount: false,
  fromAssistant: false,
  assistantDebugInfoAllowed: false,
  fromSettingsSearch: false,
  fromAutofill: false,
  autofillMetadata: '',
  traceId: 1,
  hasLinkedCrossDevicePhone: false,
};

/** @type {!FeedbackContext} */
export const fakeEmptyFeedbackContext = {
  email: '',
  pageUrl: {url: ''},
  isInternalAccount: false,
  fromAssistant: false,
  assistantDebugInfoAllowed: false,
  fromSettingsSearch: false,
  fromAutofill: false,
  autofillMetadata: '',
  traceId: 0,
  hasLinkedCrossDevicePhone: false,
};

/** @type {!FeedbackContext} */
export const fakeInternalUserFeedbackContext = {
  email: 'test.user@google.com',
  pageUrl: {url: 'chrome://tab/'},
  isInternalAccount: true,
  fromAssistant: true,
  assistantDebugInfoAllowed: false,
  fromSettingsSearch: true,
  fromAutofill: false,
  autofillMetadata: '',
  traceId: 1,
  hasLinkedCrossDevicePhone: true,
};

/** @type {!FeedbackContext} */
export const fakeFeedbackContextWithoutLinkedCrossDevicePhone = {
  email: 'test.user@google.com',
  pageUrl: {url: 'chrome://tab/'},
  isInternalAccount: true,
  fromAssistant: true,
  assistantDebugInfoAllowed: false,
  fromSettingsSearch: true,
  fromAutofill: false,
  autofillMetadata: '',
  traceId: 1,
  hasLinkedCrossDevicePhone: false,
};

/** @type {!Array<number>} */
export const fakePngData = [
  137, 80,  78, 71,  13,  10, 26, 10,  0,  0,  0,   13,  73,  72,  68,  82,
  0,   0,   0,  8,   0,   0,  0,  8,   8,  2,  0,   0,   0,   75,  109, 41,
  220, 0,   0,  0,   34,  73, 68, 65,  84, 8,  215, 99,  120, 173, 168, 135,
  21,  49,  0,  241, 255, 15, 90, 104, 8,  33, 129, 83,  7,   97,  163, 136,
  214, 129, 93, 2,   43,  2,  0,  181, 31, 90, 179, 225, 252, 176, 37,  0,
  0,   0,   0,  73,  69,  78, 68, 174, 66, 96, 130,
];
