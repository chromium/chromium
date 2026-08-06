// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

export function recordAction(actionName: string) {
  const metricsService = chrome.histograms || chrome.metricsPrivate;
  if (metricsService) {
    metricsService.recordUserAction(actionName);
    metricsService.recordBoolean(actionName, true);
  }
}

export interface UnboundedDialog extends HTMLDialogElement {
  showUnboundedElement?: () => Promise<void>;
  hideUnboundedElement?: () => Promise<void>;
}

const toggleListeners = new WeakMap<UnboundedDialog, (event: Event) => void>();

interface MenuWithDialog {
  getDialog(): HTMLDialogElement;
  close(): void;
}

export function showUnboundedMenu(
    menu: MenuWithDialog, enabled: boolean, menuName: string) {
  const dialogEl = menu.getDialog() as UnboundedDialog;
  if (enabled && dialogEl.showUnboundedElement) {
    const existingListener = toggleListeners.get(dialogEl);
    if (existingListener) {
      dialogEl.removeEventListener('unbounded', existingListener);
    }

    dialogEl.setAttribute('unbounded', '');

    const onUnboundedToggle = (event: Event) => {
      const toggleEvent = event as ToggleEvent;
      if (toggleEvent.newState === 'closed') {
        if (toggleListeners.get(dialogEl) === onUnboundedToggle) {
          dialogEl.removeAttribute('unbounded');
          dialogEl.removeEventListener('unbounded', onUnboundedToggle);
          toggleListeners.delete(dialogEl);
          menu.close();
        }
      }
    };
    toggleListeners.set(dialogEl, onUnboundedToggle);
    dialogEl.addEventListener('unbounded', onUnboundedToggle);

    dialogEl.showUnboundedElement().catch((err: unknown) => {
      console.warn(`Failed to show unbounded ${menuName} menu:`, err);
      if (toggleListeners.get(dialogEl) === onUnboundedToggle) {
        dialogEl.removeAttribute('unbounded');
        dialogEl.removeEventListener('unbounded', onUnboundedToggle);
        toggleListeners.delete(dialogEl);
      }
    });
  }
}

export function hideUnboundedMenu(
    menu: MenuWithDialog, enabled: boolean, isOpen: boolean,
    menuName: string) {
  const dialogEl = menu.getDialog() as UnboundedDialog;
  if (!isOpen && dialogEl) {
    const listener = toggleListeners.get(dialogEl);
    if (listener) {
      dialogEl.removeEventListener('unbounded', listener);
      toggleListeners.delete(dialogEl);
    }
    const wasUnbounded = dialogEl.hasAttribute('unbounded');
    if (wasUnbounded) {
      dialogEl.removeAttribute('unbounded');
      if (enabled && dialogEl.hideUnboundedElement) {
        dialogEl.hideUnboundedElement().catch((err: unknown) => {
          console.warn(`Failed to hide unbounded ${menuName} menu:`, err);
        });
      }
    }
  }
}

export function isGoogleOrigin(origin: string): boolean {
  // <if expr="not is_official_build">
  if (origin === 'http://localhost' || origin.startsWith('http://localhost:') ||
      origin === 'null' || origin.startsWith('file://')) {
    return true;
  }
  // </if>
  // Matches https://*.google.com, https://google.com, https://*.googlers.com,
  // etc. Also matches regional domains like google.co.uk, google.es, etc.
  const googleOriginRegex =
      /^https:\/\/([a-z0-9-]+\.)*(google|googlers)\.(com|[a-z]{2}|co\.[a-z]{2})(\.[a-z]{2})?$/;
  return googleOriginRegex.test(origin);
}

export function getArrayBufferFromBigBuffer(bigBuffer: BigBuffer): ArrayBuffer {
  if (bigBuffer.bytes !== undefined) {
    return new Uint8Array(bigBuffer.bytes).buffer;
  }
  if (bigBuffer.sharedMemory !== undefined) {
    return bigBuffer.sharedMemory.bufferHandle
        .mapBuffer(0, bigBuffer.sharedMemory.size)
        .buffer;
  }
  throw new Error('Invalid BigBuffer');
}

export const HANDSHAKE_INTERVAL_MS = 10;
export const HANDSHAKE_TIMEOUT_MS = 30000;
export const MAX_HANDSHAKE_ATTEMPTS =
    HANDSHAKE_TIMEOUT_MS / HANDSHAKE_INTERVAL_MS;
