// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

// clang-format on
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format off

  /** @interface */
  export class HatsBrowserProxy {
    /**
     * Helper function that initiates the launching of HaTS (Happiness Tracking
     * Surveys) through sending a request to HatsService, which is the entity
     * that decides whether it's appropriate to show a survey.
     */
    tryShowSurvey() {}

    /**
     * Helper which initiates launching of the HaTS survey for the Privacy
     * Sandbox page. A survey is only shown if the HaTS service determines it
     * is appropriate to show.
     */
    tryShowPrivacySandboxSurvey() {}
  }

  /** @implements {HatsBrowserProxy} */
  export class HatsBrowserProxyImpl {
    /** @override*/
    tryShowSurvey() {
      chrome.send('tryShowHatsSurvey');
    }

    /** @override*/
    tryShowPrivacySandboxSurvey() {
      chrome.send('tryShowPrivacySandboxSurvey');
    }
  }

  addSingletonGetter(HatsBrowserProxyImpl);

