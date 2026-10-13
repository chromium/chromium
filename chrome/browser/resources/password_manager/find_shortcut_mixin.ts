// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FindShortcutManager} from 'chrome://resources/cr_elements/find_shortcut_manager.js';
import type {FindShortcutListener} from 'chrome://resources/cr_elements/find_shortcut_manager.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * Used to determine how to handle find shortcut invocations.
 */
export const FindShortcutMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<FindShortcutListener> => {
      class FindShortcutMixin extends superClass implements
          FindShortcutListener {
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
