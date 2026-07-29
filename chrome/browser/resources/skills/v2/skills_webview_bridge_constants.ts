// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {loadTimeData} from '//resources/js/load_time_data.js';

/** Message type used by the host to initiate the handshake ping. */
export const SKILLS_HANDSHAKE_TYPE = 'skills-handshake';

/** Message type used by the guest to acknowledge the handshake. */
export const SKILLS_HANDSHAKE_ACK = 'SKILLS_HANDSHAKE_ACK';

/** Message type used by the guest to request showing a toast. */
export const SKILLS_SHOW_TOAST = 'show-toast';

/** Message type used by the guest to request invoking a skill. */
export const SKILLS_INVOKE_SKILL = 'invoke-skill';

/** Message type used by the guest to close the dialog. */
export const SKILLS_CLOSE_DIALOG = 'close-dialog';

/** Message type used by the guest to open a URL in a new tab. */
export const SKILLS_OPEN_URL = 'open-url';

/** Message type used by the host to send the Gemini prompt. */
export const SKILLS_GEMINI_PROMPT_TYPE = 'skills-gemini-prompt';

/** Query parameter key used to indicate the prompt needs to be saved. */
export const IS_SAVING_GEMINI_QUERY_PARAMETER = 'isSavingGeminiPrompt';

/** Message type used by the guest to report performance metrics. */
export const SKILLS_LOG_METRIC = 'log-metric';

/** Query parameter key used to indicate the skill is a first-party skill. */
export const IS_FIRST_PARTY_QUERY_PARAMETER = 'isFirstParty';

/**
 * Interval in milliseconds between successive handshake pings sent by the
 * host.
 */
export const HANDSHAKE_PING_INTERVAL_MS = 50;

/** Timeout in milliseconds before the host aborts the handshake. */
export const HANDSHAKE_TIMEOUT_MS = 5000;

/** Returns the primary origin for the Skills guest page. */
export function getPrimarySkillsOrigin(): string {
  return loadTimeData.getString('skillsPrimaryOrigin');
}

/** Returns the allowed origins list. */
export function getSkillsApiAllowedOrigins(): string[] {
  return [
    getPrimarySkillsOrigin(),
    'https://accounts.google.com',
    // Only allowed for internal users.
    'https://login.corp.google.com',
    'https://accounts.googlers.com',
  ];
}

/** Returns the remote URL that the webview loads. */
export function getSkillsRemoteUrl(): string {
  return `${getPrimarySkillsOrigin()}/chromeskills/browse`;
}

const REMOTE_PATH_PREFIX = '/chromeskills';

/**
 * Translates a Chrome WebUI path (e.g. '/yourSkills') to the corresponding
 * staging remote URL.
 */
export function getRemoteUrlForChromePath(chromePath: string): string {
  return `${getPrimarySkillsOrigin()}${REMOTE_PATH_PREFIX}${chromePath}`;
}

/**
 * Translates a staging remote URL back to the corresponding Chrome WebUI path
 * to display in the address bar.
 */
export function getChromePathForRemoteUrl(url: URL): string {
  if (url.origin !== getPrimarySkillsOrigin() ||
      !url.pathname.startsWith(REMOTE_PATH_PREFIX)) {
    console.warn(
        `URL "${url.href}" does not match primary ` +
        `origin or expected path prefix.`);
    return '/browse';
  }
  return url.pathname.substring(REMOTE_PATH_PREFIX.length);
}

/** Loading stages for the Webview UI. */
// LINT.IfChange(LoadingStage)
export enum LoadingStage {
  COOKIE_SYNC = 'COOKIE_SYNC',
  NAVIGATION = 'NAVIGATION',
  HANDSHAKE = 'HANDSHAKE',
  GUEST_FRAMEWORK = 'GUEST_FRAMEWORK',
  GUEST_WEB_CLIENT = 'GUEST_WEB_CLIENT',
  GUEST_DATA_FETCH = 'GUEST_DATA_FETCH',
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/histograms.xml:SkillsLoadingStage)

/** Returns the histogram name for a given loading stage. */
export function getLoadingStageHistogramName(stage: LoadingStage): string {
  return `Skills.Webview.LoadingStageDuration.${stage}`;
}

/** Non-stage histogram names used by both production code and WebUI tests. */
export const HISTOGRAM_HANDSHAKE_RESULT = 'Skills.Webview.Handshake.Result';
export const HISTOGRAM_TOTAL_INIT_LATENCY = 'Skills.Webview.TotalInitLatency';
export const HISTOGRAM_WRITE_LATENCY = 'Skills.Webview.WriteLatency';
