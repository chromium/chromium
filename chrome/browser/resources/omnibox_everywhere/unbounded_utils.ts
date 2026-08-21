// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface UnboundedElement extends HTMLDialogElement {
  showUnboundedElement?: () => Promise<void>;
  hideUnboundedElement?: () => Promise<void>;
}

export interface UnboundedToggleEvent extends Event {
  oldState?: string;
  newState?: string;
}

/**
 * Traverses a component tree across Shadow DOM boundaries to find the native
 * `<dialog>` element. Prefers component-level public accessors (like
 * `getDialog()`) when available, and falls back to shadow tree traversal.
 */
export function getContextMenuDialog(
    root: Element|ShadowRoot|null,
    entrypointSelector?: string): UnboundedElement|null {
  if (!root) {
    return null;
  }
  if (entrypointSelector) {
    const host = root.querySelector<HTMLElement>(entrypointSelector);
    if (!host) {
      return null;
    }
    if ('getDialog' in host &&
        typeof (host as {getDialog: () => unknown}).getDialog === 'function') {
      return (host as {getDialog: () => HTMLDialogElement}).getDialog() as
          UnboundedElement;
    }
    return findDialogElement(host);
  }
  if (root instanceof HTMLElement && 'getDialog' in root &&
      typeof (root as {getDialog: () => unknown}).getDialog === 'function') {
    return (root as {getDialog: () => HTMLDialogElement}).getDialog() as
        UnboundedElement;
  }
  return findDialogElement(root);
}

function findDialogElement(root: Element|ShadowRoot|null): UnboundedElement|
    null {
  if (!root) {
    return null;
  }
  if (root instanceof HTMLDialogElement) {
    return root as UnboundedElement;
  }
  const dialog = root.querySelector('dialog');
  if (dialog) {
    return dialog as UnboundedElement;
  }
  if ('shadowRoot' in root && root.shadowRoot) {
    const fromShadow = findDialogElement(root.shadowRoot);
    if (fromShadow) {
      return fromShadow;
    }
  }
  const children = root.querySelectorAll('*');
  for (const child of children) {
    if ('getDialog' in child &&
        typeof (child as {getDialog: () => unknown}).getDialog === 'function') {
      return (child as {getDialog: () => HTMLDialogElement}).getDialog() as
          UnboundedElement;
    }
    if (child.shadowRoot) {
      const nested = findDialogElement(child.shadowRoot);
      if (nested) {
        return nested;
      }
    }
  }
  return null;
}

/**
 * Handles toggling the 'unbounded' attribute and invoking the native
 * showUnboundedElement() / hideUnboundedElement() methods on an element.
 */
export function updateUnboundedElementVisibility(
    element: UnboundedElement|null, isVisible: boolean,
    onShowAsyncCheck?: () => boolean): void {
  if (!element) {
    return;
  }

  const elementName =
      element.id ? `#${element.id}` : element.tagName.toLowerCase();

  if (isVisible) {
    if (element.showUnboundedElement) {
      element.setAttribute('unbounded', '');
      const showFn = () => {
        if (onShowAsyncCheck && !onShowAsyncCheck()) {
          element.removeAttribute('unbounded');
          return;
        }
        if (element.showUnboundedElement) {
          element.showUnboundedElement().catch((err: unknown) => {
            console.warn(`Failed to show unbounded ${elementName}:`, err);
          });
        }
      };

      if (onShowAsyncCheck) {
        requestAnimationFrame(showFn);
      } else {
        showFn();
      }
    }
  } else {
    const isCurrentlyUnbounded = element.hasAttribute('unbounded');
    if (isCurrentlyUnbounded) {
      if (element.hideUnboundedElement) {
        element.hideUnboundedElement()
            .catch((err: unknown) => {
              console.warn(`Failed to hide unbounded ${elementName}:`, err);
            })
            .finally(() => {
              element.removeAttribute('unbounded');
            });
      } else {
        element.removeAttribute('unbounded');
      }
    }
  }
}

/**
 * Manages the lifecycle of an unbounded contextual menu dialog, including
 * event registration and visibility synchronization.
 */
export class UnboundedMenuManager {
  private getContextElement_: () => HTMLElement | null;
  private onUnboundedClosedCallback_?: () => void;

  constructor(
      getContextElement: () => HTMLElement | null,
      onUnboundedClosed?: () => void) {
    this.getContextElement_ = getContextElement;
    this.onUnboundedClosedCallback_ = onUnboundedClosed;
  }

  getDialog(): UnboundedElement|null {
    const contextEl = this.getContextElement_();
    return getContextMenuDialog(contextEl);
  }

  isDialogOpen(): boolean {
    const dialog = this.getDialog();
    return Boolean(dialog && dialog.open);
  }

  onContextMenuOpened(): void {
    const dialog = this.getDialog();
    if (dialog) {
      dialog.addEventListener('beforetoggle', this.onToggle_);
      updateUnboundedElementVisibility(dialog, true);
    }
  }

  onContextMenuClosed(): void {
    const dialog = this.getDialog();
    if (dialog) {
      dialog.removeEventListener('beforetoggle', this.onToggle_);
      updateUnboundedElementVisibility(dialog, false);
    }
  }

  private onToggle_ = (e: Event) => {
    const toggleEvent = e as UnboundedToggleEvent;
    if (toggleEvent.newState === 'closed') {
      const contextEl = this.getContextElement_();
      if (contextEl && 'closeMenu' in contextEl &&
          typeof (contextEl as {closeMenu: () => void}).closeMenu ===
              'function') {
        (contextEl as {closeMenu: () => void}).closeMenu();
      }
      this.onUnboundedClosedCallback_?.();
    }
  };
}
