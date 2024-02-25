// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * A user interest to display. There must only be one of |topic| or |site| set.
 */
export interface PrivacySandboxInterest {
  removed: boolean;
  topic?: CanonicalTopic;
  site?: string;
}

export interface FledgeState {
  joiningSites: string[];
  blockedSites: string[];
}

/**
 * The canonical form of a Topics API topic. Must be kept in sync with the
 * version at components/privacy_sandbox/canonical_topic.h.
 */
export interface CanonicalTopic {
  topicId: number;
  taxonomyVersion: number;
  displayString: string;
  description: string;
}

export interface TopicsState {
  topTopics: CanonicalTopic[];
  blockedTopics: CanonicalTopic[];
}

export interface FirstLevelTopicsState {
  firstLevelTopics: CanonicalTopic[];
  blockedTopics: CanonicalTopic[];
}

export interface PrivacySandboxBrowserProxy {
  /** Retrieves the user's current FLEDGE state. */
  getFledgeState(): Promise<FledgeState>;

  /** Sets FLEDGE joining to |allowed| for |site|.*/
  setFledgeJoiningAllowed(site: string, allowed: boolean): void;

  /** Retrieves the user's current Topics state. */
  getTopicsState(): Promise<TopicsState>;

  /** Sets |topic| to |allowed| for the Topics API.*/
  setTopicAllowed(topic: CanonicalTopic, allowed: boolean): void;

  /**
   * Informs the Privacy Sandbox Service that the user interacted with the
   * Topics toggle.
   */
  topicsToggleChanged(newToggleValue: boolean): void;

  /**
   * Proactive Topics Blocking - used to get the full list of first level
   * topics.
   */
  getFirstLevelTopics(): Promise<FirstLevelTopicsState>;

  /**
   * Proactive Topics Blocking - used to see if the passed in topic has any
   *  child topics that are currently assigned
   */
  getChildTopicsCurrentlyAssigned(topic: CanonicalTopic):
      Promise<CanonicalTopic[]>;
}

export class PrivacySandboxBrowserProxyImpl implements
    PrivacySandboxBrowserProxy {
  getFledgeState() {
    return sendWithPromise('getFledgeState');
  }

  setFledgeJoiningAllowed(site: string, allowed: boolean) {
    chrome.send('setFledgeJoiningAllowed', [site, allowed]);
  }

  getTopicsState() {
    return sendWithPromise('getTopicsState');
  }

  setTopicAllowed(topic: CanonicalTopic, allowed: boolean) {
    chrome.send(
        'setTopicAllowed', [topic.topicId, topic.taxonomyVersion, allowed]);
  }

  topicsToggleChanged(newToggleValue: boolean) {
    chrome.send('topicsToggleChanged', [newToggleValue]);
  }

  getFirstLevelTopics() {
    return sendWithPromise('getFirstLevelTopics');
  }

  getChildTopicsCurrentlyAssigned(topic: CanonicalTopic) {
    return sendWithPromise(
        'getChildTopicsCurrentlyAssigned', topic.topicId,
        topic.taxonomyVersion);
  }

  static getInstance(): PrivacySandboxBrowserProxy {
    return instance || (instance = new PrivacySandboxBrowserProxyImpl());
  }

  static setInstance(obj: PrivacySandboxBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacySandboxBrowserProxy|null = null;
