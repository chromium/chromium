// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.downloads API
 * Partially generated from:
 * chrome/common/extensions/api/downloads.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/downloads.idl -g ts_definitions` to regenerate.
 */

import type {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace downloads {

      export interface HeaderNameValuePair {
        name: string;
        value: string;
      }

      export enum FilenameConflictAction {
        UNIQUIFY = 'uniquify',
        OVERWRITE = 'overwrite',
        PROMPT = 'prompt',
      }

      export interface FilenameSuggestion {
        filename: string;
        conflictAction?: FilenameConflictAction;
      }

      export enum HttpMethod {
        GET = 'GET',
        POST = 'POST',
      }

      export enum InterruptReason {
        FILE_FAILED = 'FILE_FAILED',
        FILE_ACCESS_DENIED = 'FILE_ACCESS_DENIED',
        FILE_NO_SPACE = 'FILE_NO_SPACE',
        FILE_NAME_TOO_LONG = 'FILE_NAME_TOO_LONG',
        FILE_TOO_LARGE = 'FILE_TOO_LARGE',
        FILE_VIRUS_INFECTED = 'FILE_VIRUS_INFECTED',
        FILE_TRANSIENT_ERROR = 'FILE_TRANSIENT_ERROR',
        FILE_BLOCKED = 'FILE_BLOCKED',
        FILE_SECURITY_CHECK_FAILED = 'FILE_SECURITY_CHECK_FAILED',
        FILE_TOO_SHORT = 'FILE_TOO_SHORT',
        FILE_HASH_MISMATCH = 'FILE_HASH_MISMATCH',
        FILE_SAME_AS_SOURCE = 'FILE_SAME_AS_SOURCE',
        NETWORK_FAILED = 'NETWORK_FAILED',
        NETWORK_TIMEOUT = 'NETWORK_TIMEOUT',
        NETWORK_DISCONNECTED = 'NETWORK_DISCONNECTED',
        NETWORK_SERVER_DOWN = 'NETWORK_SERVER_DOWN',
        NETWORK_INVALID_REQUEST = 'NETWORK_INVALID_REQUEST',
        SERVER_FAILED = 'SERVER_FAILED',
        SERVER_NO_RANGE = 'SERVER_NO_RANGE',
        SERVER_BAD_CONTENT = 'SERVER_BAD_CONTENT',
        SERVER_UNAUTHORIZED = 'SERVER_UNAUTHORIZED',
        SERVER_CERT_PROBLEM = 'SERVER_CERT_PROBLEM',
        SERVER_FORBIDDEN = 'SERVER_FORBIDDEN',
        SERVER_UNREACHABLE = 'SERVER_UNREACHABLE',
        SERVER_CONTENT_LENGTH_MISMATCH = 'SERVER_CONTENT_LENGTH_MISMATCH',
        SERVER_CROSS_ORIGIN_REDIRECT = 'SERVER_CROSS_ORIGIN_REDIRECT',
        USER_CANCELED = 'USER_CANCELED',
        USER_SHUTDOWN = 'USER_SHUTDOWN',
        CRASH = 'CRASH',
      }

      export interface DownloadOptions {
        url: string;
        filename?: string;
        conflictAction?: FilenameConflictAction;
        saveAs?: boolean;
        method?: HttpMethod;
        headers?: HeaderNameValuePair[];
        body?: string;
      }

      export enum DangerType {
        FILE = 'file',
        URL = 'url',
        CONTENT = 'content',
        UNCOMMON = 'uncommon',
        HOST = 'host',
        UNWANTED = 'unwanted',
        SAFE = 'safe',
        ACCEPTED = 'accepted',
        ALLOWLISTED_BY_POLICY = 'allowlistedByPolicy',
        ASYNC_SCANNING = 'asyncScanning',
        ASYNC_LOCAL_PASSWORD_SCANNING = 'asyncLocalPasswordScanning',
        PASSWORD_PROTECTED = 'passwordProtected',
        BLOCKED_TOO_LARGE = 'blockedTooLarge',
        SENSITIVE_CONTENT_WARNING = 'sensitiveContentWarning',
        SENSITIVE_CONTENT_BLOCK = 'sensitiveContentBlock',
        DEEP_SCANNED_FAILED = 'deepScannedFailed',
        DEEP_SCANNED_SAFE = 'deepScannedSafe',
        DEEP_SCANNED_OPENED_DANGEROUS = 'deepScannedOpenedDangerous',
        PROMPT_FOR_SCANNING = 'promptForScanning',
        PROMPT_FOR_LOCAL_PASSWORD_SCANNING = 'promptForLocalPasswordScanning',
        ACCOUNT_COMPROMISE = 'accountCompromise',
        BLOCKED_SCAN_FAILED = 'blockedScanFailed',
      }

      export enum State {
        IN_PROGRESS = 'in_progress',
        INTERRUPTED = 'interrupted',
        COMPLETE = 'complete',
      }

      export interface DownloadItem {
        id: number;
        url: string;
        finalUrl: string;
        referrer: string;
        filename: string;
        incognito: boolean;
        danger: DangerType;
        mime: string;
        startTime: string;
        endTime?: string;
        estimatedEndTime?: string;
        state: State;
        paused: boolean;
        canResume: boolean;
        error?: InterruptReason;
        bytesReceived: number;
        totalBytes: number;
        fileSize: number;
        exists: boolean;
        byExtensionId?: string;
        byExtensionName?: string;
      }

      export interface DownloadQuery {
        query?: string[];
        startedBefore?: string;
        startedAfter?: string;
        endedBefore?: string;
        endedAfter?: string;
        totalBytesGreater?: number;
        totalBytesLess?: number;
        filenameRegex?: string;
        urlRegex?: string;
        finalUrlRegex?: string;
        limit?: number;
        orderBy?: string[];
        id?: number;
        url?: string;
        finalUrl?: string;
        filename?: string;
        danger?: DangerType;
        mime?: string;
        startTime?: string;
        endTime?: string;
        state?: State;
        paused?: boolean;
        error?: InterruptReason;
        bytesReceived?: number;
        totalBytes?: number;
        fileSize?: number;
        exists?: boolean;
      }

      export interface StringDelta {
        previous?: string;
        current?: string;
      }

      export interface DoubleDelta {
        previous?: number;
        current?: number;
      }

      export interface BooleanDelta {
        previous?: boolean;
        current?: boolean;
      }

      export interface DownloadDelta {
        id: number;
        url?: StringDelta;
        finalUrl?: StringDelta;
        filename?: StringDelta;
        danger?: StringDelta;
        mime?: StringDelta;
        startTime?: StringDelta;
        endTime?: StringDelta;
        state?: StringDelta;
        canResume?: BooleanDelta;
        paused?: BooleanDelta;
        error?: StringDelta;
        totalBytes?: DoubleDelta;
        fileSize?: DoubleDelta;
        exists?: BooleanDelta;
      }

      export interface GetFileIconOptions {
        size?: number;
      }

      export interface UiOptions {
        enabled: boolean;
      }

      export function download(options: DownloadOptions): Promise<number>;

      export function search(
          query: DownloadQuery,
          callback: (results: DownloadItem[]) => void): Promise<DownloadItem[]>;

      export function pause(downloadId: number): Promise<void>;

      export function resume(downloadId: number): Promise<void>;

      export function cancel(downloadId: number): Promise<void>;

      export function getFileIcon(
          downloadId: number,
          options?: GetFileIconOptions): Promise<string|undefined>;

      export function open(downloadId: number): Promise<void>;

      export function show(downloadId: number): void;

      export function showDefaultFolder(): void;

      export function erase(query: DownloadQuery): Promise<number[]>;

      export function removeFile(downloadId: number): Promise<void>;

      export function acceptDanger(downloadId: number): Promise<void>;

      export function setShelfEnabled(enabled: boolean): void;

      export function setUiOptions(options: UiOptions): Promise<void>;

      export const onCreated: ChromeEvent<(downloadItem: DownloadItem) => void>;

      export const onErased: ChromeEvent<(downloadId: number) => void>;

      export const onChanged:
          ChromeEvent<(downloadDelta: DownloadDelta) => void>;

      export const onDeterminingFilename: ChromeEvent<
          (downloadItem: DownloadItem,
           suggest: (suggestion?: FilenameSuggestion) => void) => void>;

    }
  }
}
