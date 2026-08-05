// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function recordAction(actionName: string) {
  chrome.histograms.recordUserAction(actionName);
  chrome.histograms.recordBoolean(actionName, true);
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
