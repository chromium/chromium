// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CrContainerShadowMixin holds logic for showing a drop shadow
 * near the top of a container element, when the content has scrolled.
 *
 * Forked from ui/webui/resources/cr_elements/cr_container_shadow_mixin.ts
 *
 * Elements using this mixin are expected to define a #container element,
 * which is the element being scrolled. If the #container element has a
 * show-bottom-shadow attribute, a drop shadow will also be shown near the
 * bottom of the container element, when there is additional content to scroll
 * to. Examples:
 *
 * For both top and bottom shadows:
 * <div id="container" show-bottom-shadow>...</div>
 *
 * For top shadow only:
 * <div id="container">...</div>
 *
 * The mixin will take care of inserting an element with ID
 * 'cr-container-shadow-top' which holds the drop shadow effect, and,
 * optionally, an element with ID 'cr-container-shadow-bottom' which holds the
 * same effect. A 'has-shadow' CSS class is automatically added to/removed from
 * both elements while scrolling, as necessary. Note that the show-bottom-shadow
 * attribute is inspected only during attached(), and any changes to it that
 * occur after that point will not be respected.
 *
 * Clients should either use the existing shared styling in
 * cr_shared_style.css, '#cr-container-shadow-[top/bottom]' and
 * '#cr-container-shadow-[top/bottom].has-shadow', or define their own styles.
 */

import {assert} from '//resources/js/assert.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export enum CrContainerShadowSide {
  TOP = 'top',
  BOTTOM = 'bottom',
}

type Constructor<T> = new (...args: any[]) => T;

export const CrContainerShadowMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrContainerShadowMixinInterface> => {
      class CrContainerShadowMixin extends superClass implements
          CrContainerShadowMixinInterface {
        private intersectionObserver_: IntersectionObserver|null = null;
        private dropShadows_: Map<CrContainerShadowSide, HTMLDivElement> =
            new Map();
        private intersectionProbes_:
            Map<CrContainerShadowSide, HTMLDivElement> = new Map();
        private sides_: CrContainerShadowSide[]|null = null;

        override connectedCallback() {
          super.connectedCallback();

          const hasBottomShadow =
              this.getContainer_().hasAttribute('show-bottom-shadow');
          this.sides_ = hasBottomShadow ?
              [CrContainerShadowSide.TOP, CrContainerShadowSide.BOTTOM] :
              [CrContainerShadowSide.TOP];
          this.sides_!.forEach(side => {
            // The element holding the drop shadow effect to be shown.
            const shadow = document.createElement('div');
            shadow.id = `cr-container-shadow-${side}`;
            shadow.classList.add('cr-container-shadow');
            this.dropShadows_.set(side, shadow);
            this.intersectionProbes_.set(side, document.createElement('div'));
          });

          this.getContainer_().parentNode!.insertBefore(
              this.dropShadows_.get(CrContainerShadowSide.TOP)!,
              this.getContainer_());
          this.getContainer_().prepend(
              this.intersectionProbes_.get(CrContainerShadowSide.TOP)!);

          if (hasBottomShadow) {
            this.getContainer_().parentNode!.insertBefore(
                this.dropShadows_.get(CrContainerShadowSide.BOTTOM)!,
                this.getContainer_().nextSibling);
            this.getContainer_().append(
                this.intersectionProbes_.get(CrContainerShadowSide.BOTTOM)!);
          }

          this.enableShadowBehavior(true);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          this.enableShadowBehavior(false);
        }

        private getContainer_(): HTMLElement {
          return this.shadowRoot!.querySelector('#container')!;
        }

        private getIntersectionObserver_(): IntersectionObserver {
          const callback = (entries: IntersectionObserverEntry[]) => {
            // In some rare cases, there could be more than one entry per
            // observed element, in which case the last entry's result
            // stands.
            for (const entry of entries) {
              const target = entry.target;
              this.sides_!.forEach(side => {
                if (target === this.intersectionProbes_.get(side)) {
                  this.dropShadows_.get(side)!.classList.toggle(
                      'has-shadow', entry.intersectionRatio === 0);
                }
              });
            }
          };
          return new IntersectionObserver(
              callback, {root: this.getContainer_(), threshold: 0});
        }

        /**
         * @param enable Whether to enable the mixin or disable it.
         *     This function does nothing if the mixin is already in the
         *     requested state.
         */
        enableShadowBehavior(enable: boolean) {
          // Behavior is already enabled/disabled. Return early.
          if (enable === !!this.intersectionObserver_) {
            return;
          }

          if (!enable) {
            this.intersectionObserver_!.disconnect();
            this.intersectionObserver_ = null;
            return;
          }

          this.intersectionObserver_ = this.getIntersectionObserver_();

          // Need to register the observer within a setTimeout() callback,
          // otherwise the drop shadow flashes once on startup, because of the
          // DOM modifications earlier in this function causing a relayout.
          window.setTimeout(() => {
            if (this.intersectionObserver_) {
              // In case this is already detached.
              this.intersectionProbes_.forEach(probe => {
                this.intersectionObserver_!.observe(probe);
              });
            }
          });
        }

        /**
         * Shows the shadows. The shadow mixin must be disabled before
         * calling this method, otherwise the intersection observer might
         * show the shadows again.
         */
        showDropShadows() {
          assert(!this.intersectionObserver_);
          assert(this.sides_);
          for (const side of this.sides_) {
            this.dropShadows_.get(side)!.classList.toggle('has-shadow', true);
          }
        }
      }

      return CrContainerShadowMixin;
    });

export interface CrContainerShadowMixinInterface {
  enableShadowBehavior(enable: boolean): void;

  showDropShadows(): void;
}
