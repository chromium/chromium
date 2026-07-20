// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Message type used by the host to initiate the handshake ping. */
export const SKILLS_HANDSHAKE_TYPE = 'skills-handshake';

/** Message type used by the guest to acknowledge the handshake. */
export const SKILLS_HANDSHAKE_ACK = 'SKILLS_HANDSHAKE_ACK';

/** Message type used by the guest to request showing a toast. */
export const SKILLS_SHOW_TOAST = 'show-toast';

/** Message type used by the guest to request invoking a skill. */
export const SKILLS_INVOKE_SKILL = 'invoke-skill';

/**
 * Interval in milliseconds between successive handshake pings sent by the
 * host.
 */
export const HANDSHAKE_PING_INTERVAL_MS = 50;

/** Timeout in milliseconds before the host aborts the handshake. */
export const HANDSHAKE_TIMEOUT_MS = 5000;

/** The primary origin for the Skills guest page. */
export const PRIMARY_SKILLS_ORIGIN =
    'https://chromeskills-staging.corp.google.com';

export const SKILLS_API_ALLOWED_ORIGINS = [
  PRIMARY_SKILLS_ORIGIN,
  'https://accounts.google.com',
  // Only allowed for internal users.
  'https://login.corp.google.com',
  'https://accounts.googlers.com',
];

/** The remote URL that the webview loads. */
export const SKILLS_REMOTE_URL = `${PRIMARY_SKILLS_ORIGIN}/chromeskills/browse`;

const REMOTE_PATH_PREFIX = '/chromeskills';

/**
 * Translates a Chrome WebUI path (e.g. '/yourSkills') to the corresponding
 * staging remote URL.
 */
export function getRemoteUrlForChromePath(chromePath: string): string {
  return `${PRIMARY_SKILLS_ORIGIN}${REMOTE_PATH_PREFIX}${chromePath}`;
}

/**
 * Translates a staging remote URL back to the corresponding Chrome WebUI path
 * to display in the address bar.
 */
export function getChromePathForRemoteUrl(url: URL): string {
  if (url.origin !== PRIMARY_SKILLS_ORIGIN ||
      !url.pathname.startsWith(REMOTE_PATH_PREFIX)) {
    console.warn(
        `URL "${url.href}" does not match primary ` +
        `origin or expected path prefix.`);
    return '/browse';
  }
  return url.pathname.substring(REMOTE_PATH_PREFIX.length);
}
