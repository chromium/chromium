// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin for scrollable containers with <iron-list>.
 *
 * Any containers with the 'scrollable' attribute set will have the following
 * classes toggled appropriately: can-scroll, is-scrolled, scrolled-to-bottom.
 * These classes are used to style the container div and list elements
 * appropriately, see cr_shared_style.css.
 *
 * The associated HTML should look something like:
 *   <div id="container" scrollable>
 *     <iron-list items="[[items]]" scroll-target="container">
 *       <template>
 *         <my-element item="[[item]] tabindex$="[[tabIndex]]"></my-element>
 *       </template>
 *     </iron-list>
 *   </div>
 *
 * In order to get correct keyboard focus (tab) behavior within the list,
 * any elements with tabbable sub-elements also need to set tabindex, e.g:
 *
 * <dom-module id="my-element>
 *   <template>
 *     ...
 *     <paper-icon-button toggles active="{{opened}}" tabindex$="[[tabindex]]">
 *   </template>
 * </dom-module>
 *
 * NOTE: If 'container' is not fixed size, it is important to call
 * updateScrollableContents() when [[items]] changes, otherwise the container
 * will not be sized correctly.
 */

// clang-format off
import {beforeNextRender, dedupingMixin, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronListElement} from '//resources/polymer/v3_0/iron-list/iron-list.js';
// clang-format on

type IronListElementWithExtras = IronListElement&{
  savedScrollTops: number[],
};

type Constructor<T> = new (...args: any[]) => T;

export const CrScrollableMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrScrollableMixinInterface> => {
      class CrScrollableMixin extends superClass implements
          CrScrollableMixinInterface {
        private resizeObserver_: ResizeObserver;

        constructor(...args: any[]) {
          super(...args);

          this.resizeObserver_ = new ResizeObserver((entries) => {
            requestAnimationFrame(() => {
              for (const entry of entries) {
                this.onScrollableContainerResize_(entry.target as HTMLElement);
              }
            });
          });
        }

        override ready() {
          super.ready();

          beforeNextRender(this, () => {
            this.requestUpdateScroll();

            // Listen to the 'scroll' event for each scrollable container.
            const scrollableElements =
                this.shadowRoot!.querySelectorAll('[scrollable]');
            for (const scrollableElement of scrollableElements) {
              scrollableElement.addEventListener(
                  'scroll', this.updateScrollEvent_.bind(this));
            }
          });
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          this.resizeObserver_.disconnect();
        }

        /**
         * Called any time the contents of a scrollable container may have
         * changed. This ensures that the <iron-list> contents of dynamically
         * sized containers are resized correctly.
         */
        updateScrollableContents() {
          this.requestUpdateScroll();

          const ironLists = this.shadowRoot!.querySelectorAll<IronListElement>(
              '[scrollable] iron-list');

          for (const ironList of ironLists) {
            // When the scroll-container of an iron-list has scrollHeight of 1,
            // the iron-list will default to showing a minimum of 3 items.
            // After an iron-resize is fired, it will resize to have the correct
            // scrollHeight, but another iron-resize is required to render all
            // the items correctly.
            // If the scrollHeight of the scroll-container is 0, the element is
            // not yet rendered, and we must wait until its scrollHeight becomes
            // 1, then fire the first iron-resize event.
            const scrollContainer = ironList.parentElement!;
            const scrollHeight = scrollContainer.scrollHeight;

            if (scrollHeight <= 1 && ironList.items!.length > 0 &&
                window.getComputedStyle(scrollContainer).display !== 'none') {
              // The scroll-container does not have a proper scrollHeight yet.
              // An additional iron-resize is needed, which will be triggered by
              // the observer after scrollHeight changes.
              // Do not observe for resize if there are no items, or if the
              // scroll-container is explicitly hidden, as in those cases there
              // will not be any future resizes.
              this.resizeObserver_.observe(scrollContainer);
            }

            if (scrollHeight !== 0) {
              // If the iron-list is already rendered, fire an initial
              // iron-resize event. Otherwise, the resizeObserver_ will handle
              // firing the iron-resize event, upon its scrollHeight becoming 1.
              ironList.notifyResize();
            }
          }
        }

        /**
         * Setup the initial scrolling related classes for each scrollable
         * container. Called from ready() and updateScrollableContents(). May
         * also be called directly when the contents change (e.g. when not using
         * iron-list).
         */
        requestUpdateScroll() {
          requestAnimationFrame(() => {
            const scrollableElements =
                this.shadowRoot!.querySelectorAll<HTMLElement>('[scrollable]');
            for (const scrollableElement of scrollableElements) {
              this.updateScroll_(scrollableElement);
            }
          });
        }

        saveScroll(list: IronListElementWithExtras) {
          // Store a FIFO of saved scroll positions so that multiple updates in
          // a frame are applied correctly. Specifically we need to track when
          // '0' is saved (but not apply it), and still handle patterns like
          // [30, 0, 32].
          list.savedScrollTops = list.savedScrollTops || [];
          list.savedScrollTops.push(list.scrollTarget!.scrollTop);
        }

        restoreScroll(list: IronListElementWithExtras) {
          microTask.run(() => {
            const scrollTop = list.savedScrollTops.shift();
            // Ignore scrollTop of 0 in case it was intermittent (we do not need
            // to explicitly scroll to 0).
            if (scrollTop !== 0) {
              list.scroll(0, scrollTop!);
            }
          });
        }

        /**
         * Event wrapper for updateScroll_.
         */
        private updateScrollEvent_(event: Event) {
          const scrollable = event.target as HTMLElement;
          this.updateScroll_(scrollable);
        }

        /**
         * This gets called once initially and any time a scrollable container
         * scrolls.
         */
        private updateScroll_(scrollable: HTMLElement) {
          scrollable.classList.toggle(
              'can-scroll', scrollable.clientHeight < scrollable.scrollHeight);
          scrollable.classList.toggle('is-scrolled', scrollable.scrollTop > 0);
          scrollable.classList.toggle(
              'scrolled-to-bottom',
              scrollable.scrollTop + scrollable.clientHeight >=
                  scrollable.scrollHeight);
        }

        /**
         * This gets called upon a resize event on the scrollable element
         */
        private onScrollableContainerResize_(scrollable: HTMLElement) {
          const nodeList =
              scrollable.querySelectorAll<IronListElement>('iron-list');
          if (nodeList.length === 0 || scrollable.scrollHeight > 1) {
            // Stop observing after the scrollHeight has its correct value, or
            // if somehow there are no more iron-lists in the scrollable.
            this.resizeObserver_.unobserve(scrollable);
          }

          if (scrollable.scrollHeight !== 0) {
            // Fire iron-resize event only if scrollHeight has changed from 0 to
            // 1 or from 1 to the correct size. ResizeObserver doesn't exactly
            // observe scrollHeight and may fire despite it staying at 0, so
            // we can ignore those events.
            for (const node of nodeList) {
              node.notifyResize();
            }
          }
        }
      }
      return CrScrollableMixin;
    });

export interface CrScrollableMixinInterface {
  updateScrollableContents(): void;
  requestUpdateScroll(): void;
  saveScroll(list: IronListElement): void;
  restoreScroll(list: IronListElement): void;
}
