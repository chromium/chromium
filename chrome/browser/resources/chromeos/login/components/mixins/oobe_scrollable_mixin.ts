// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'OobeScrollableMixin' is a special mixin which helps to update classes
 * on the scrollable element on size change.
 */

type Constructor<T> = new (...args: any[]) => T;

export const OobeScrollableMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<OobeScrollableMixinInterface> => {
      class OobeScrollableMixinInternal extends superClass implements
          OobeScrollableMixinInterface {
        private scrollableElement: HTMLElement|null;
        private resizeObserver: ResizeObserver|null;

        /**
         * Init observers to keep track of the scrollable element size changes.
         */
        initScrollableObservers(scrollableElementParam: Element,
            ...sizeChangeObservableElements: Element[]): void {
          if (!(scrollableElementParam instanceof HTMLElement) ||
              this.scrollableElement) {
            return;
          }
          this.scrollableElement = scrollableElementParam;
          this.resizeObserver =
              new ResizeObserver(this.applyScrollClassTags.bind(this));
          this.scrollableElement.addEventListener(
            'scroll', this.applyScrollClassTags.bind(this));
          this.resizeObserver.observe(this.scrollableElement);
          for (const elem of sizeChangeObservableElements) {
            this.resizeObserver.observe(elem);
          }
        }

        /**
         * Applies the class tags to topScrollContainer that control the
         * shadows.
         */
        applyScrollClassTags(): void {
          const el = this.scrollableElement;
          if (el instanceof HTMLElement) {
            el.classList.toggle('can-scroll',
                el.clientHeight < el.scrollHeight);
            el.classList.toggle('is-scrolled', el.scrollTop > 0);
            el.classList.toggle('scrolled-to-bottom',
                el.scrollTop + el.clientHeight >= el.scrollHeight);
          }
        }

        /**
         * Scroll to the bottom of scrollable element.
         */
        scrollToBottom(): void {
          if (this.scrollableElement instanceof HTMLElement) {
            this.scrollableElement.scrollTop =
              this.scrollableElement.scrollHeight;
          }
        }
      }

      return OobeScrollableMixinInternal;
    });

export interface OobeScrollableMixinInterface {
  initScrollableObservers(scrollableElement: Element,
    ...sizeChangeObservableElements: Element[]): void;
  applyScrollClassTags(): void;
  scrollToBottom(): void;
}
