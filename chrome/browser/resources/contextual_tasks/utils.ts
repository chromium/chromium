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

interface MenuWithDialog {
  getDialog(): HTMLDialogElement;
}

export function showUnboundedMenu(
    menu: MenuWithDialog, enabled: boolean, menuName: string) {
  const dialogEl = menu.getDialog() as UnboundedDialog;
  if (enabled && dialogEl.showUnboundedElement) {
    dialogEl.setAttribute('unbounded', '');
    dialogEl.showUnboundedElement().catch((err: unknown) => {
      console.error(`Failed to show unbounded ${menuName} menu:`, err);
    });
  }
}

export function hideUnboundedMenu(
    menu: MenuWithDialog, enabled: boolean, isOpen: boolean,
    menuName: string) {
  const dialogEl = menu.getDialog() as UnboundedDialog;
  if (!isOpen && dialogEl && enabled && dialogEl.hideUnboundedElement) {
    dialogEl.hideUnboundedElement()
        .catch((err: unknown) => {
          console.error(`Failed to hide unbounded ${menuName} menu:`, err);
        })
        .finally(() => {
          dialogEl.removeAttribute('unbounded');
        });
  }
}
