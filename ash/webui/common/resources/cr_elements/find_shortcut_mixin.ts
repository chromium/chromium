// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {KeyboardShortcutList} from '//resources/js/keyboard_shortcut_list.js';
import {isMac} from '//resources/js/platform.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview Listens for a find keyboard shortcut (i.e. Ctrl/Cmd+f or /)
 * and keeps track of an stack of potential listeners. Only the listener at the
 * top of the stack will be notified that a find shortcut has been invoked.
 */

export const FindShortcutManager = (() => {
  /**
   * Stack of listeners. Only the top listener will handle the shortcut.
   */
  const listeners: FindShortcutMixinInterface[] = [];

  /**
   * Tracks if any modal context is open in settings. This assumes only one
   * modal can be open at a time. The modals that are being tracked include
   * cr-dialog and cr-drawer.
   * @type {boolean}
   */
  let modalContextOpen = false;

  const shortcutCtrlF = new KeyboardShortcutList(isMac ? 'meta|f' : 'ctrl|f');
  const shortcutSlash = new KeyboardShortcutList('/');

  window.addEventListener('keydown', e => {
    if (e.defaultPrevented || listeners.length === 0) {
      return;
    }

    const element = e.composedPath()[0] as Element;
    if (!shortcutCtrlF.matchesEvent(e) &&
        (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA' ||
         !shortcutSlash.matchesEvent(e))) {
      return;
    }

    const focusIndex =
        listeners.findIndex(listener => listener.searchInputHasFocus());
    // If no listener has focus or the first (outer-most) listener has focus,
    // try the last (inner-most) listener.
    // If a listener has a search input with focus, the next listener that
    // should be called is the right before it in |listeners| such that the
    // goes from inner-most to outer-most.
    const index = focusIndex <= 0 ? listeners.length - 1 : focusIndex - 1;
    if (listeners[index]!.handleFindShortcut(modalContextOpen)) {
      e.preventDefault();
    }
  });

  window.addEventListener('cr-dialog-open', () => {
    modalContextOpen = true;
  });

  window.addEventListener('cr-drawer-opened', () => {
    modalContextOpen = true;
  });

  window.addEventListener('close', e => {
    if (['CR-DIALOG', 'CR-DRAWER'].includes(
            (e.composedPath()[0] as Element).nodeName)) {
      modalContextOpen = false;
    }
  });

  return Object.freeze({listeners: listeners});
})();

type Constructor<T> = new (...args: any[]) => T;

/**
 * Used to determine how to handle find shortcut invocations.
 */
export const FindShortcutMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<FindShortcutMixinInterface> => {
      class FindShortcutMixin extends superClass implements
          FindShortcutMixinInterface {
        findShortcutListenOnAttach: boolean = true;

        override connectedCallback() {
          super.connectedCallback();
          if (this.findShortcutListenOnAttach) {
            this.becomeActiveFindShortcutListener();
          }
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          if (this.findShortcutListenOnAttach) {
            this.removeSelfAsFindShortcutListener();
          }
        }

        becomeActiveFindShortcutListener() {
          const listeners = FindShortcutManager.listeners;
          assert(
              !listeners.includes(this),
              'Already listening for find shortcuts.');
          listeners.push(this);
        }

        private handleFindShortcutInternal_() {
          assertNotReached('Must override handleFindShortcut()');
        }

        handleFindShortcut(_modalContextOpen: boolean) {
          this.handleFindShortcutInternal_();
          return false;
        }

        removeSelfAsFindShortcutListener() {
          const listeners = FindShortcutManager.listeners;
          const index = listeners.indexOf(this);
          assert(listeners.includes(this), 'Find shortcut listener not found.');
          listeners.splice(index, 1);
        }

        private searchInputHasFocusInternal_() {
          assertNotReached('Must override searchInputHasFocus()');
        }

        searchInputHasFocus() {
          this.searchInputHasFocusInternal_();
          return false;
        }
      }

      return FindShortcutMixin;
    });

export interface FindShortcutMixinInterface {
  findShortcutListenOnAttach: boolean;
  becomeActiveFindShortcutListener(): void;

  /** If handled, return true. */
  handleFindShortcut(modalContextOpen: boolean): boolean;

  removeSelfAsFindShortcutListener(): void;
  searchInputHasFocus(): boolean;
}
