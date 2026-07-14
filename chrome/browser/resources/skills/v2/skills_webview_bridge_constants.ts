// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Message type used by the host to initiate the handshake ping. */
export const SKILLS_HANDSHAKE_TYPE = 'skills-handshake';

/** Message type used by the guest to acknowledge the handshake. */
export const SKILLS_HANDSHAKE_ACK = 'SKILLS_HANDSHAKE_ACK';

/** Message type used by the guest to request showing a toast. */
export const SKILLS_SHOW_TOAST = 'show-toast';

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

/** The host URL that the guest webview loads. */
export const SKILLS_HOST_URL = `${PRIMARY_SKILLS_ORIGIN}/chromeskills/browse`;

/** The dialog URL that the guest webview loads. */
export const SKILLS_DIALOG_HOST_URL =
    `${PRIMARY_SKILLS_ORIGIN}/chromeskills/create-skill-form`;
