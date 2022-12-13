// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CrContainerShadowBehavior holds logic for showing a drop shadow
 * near the top of a container element, when the content has scrolled.
 *
 * Elements using this behavior are expected to define a #container element,
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
 * The behavior will take care of inserting an element with ID
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
 *
 * NOTE: This file is deprecated in favor of cr_container_shadow_mixin.js. Don't
 * use it in any new code.
 */

// clang-format off
import {assert} from '//resources/ash/common/assert.js';
// clang-format on

/** @enum {string} */
export const CrContainerShadowSide = {
  TOP: 'top',
  BOTTOM: 'bottom',
};

/** @polymerBehavior */
export const CrContainerShadowBehavior = {
  /** @private {?IntersectionObserver} */
  intersectionObserver_: null,

  /** @private {?Map<!CrContainerShadowSide, !HTMLDivElement>} */
  dropShadows_: null,

  /** @private {?Map<!CrContainerShadowSide, !HTMLDivElement>} */
  intersectionProbes_: null,

  /** @private {?Array<!CrContainerShadowSide>} */
  sides_: null,

  /** @override */
  ready() {
    this.dropShadows_ = new Map();
    this.intersectionProbes_ = new Map();
  },

  /** @override */
  attached() {
    const hasBottomShadow = this.$.container.hasAttribute('show-bottom-shadow');
    this.sides_ = hasBottomShadow ?
        [CrContainerShadowSide.TOP, CrContainerShadowSide.BOTTOM] :
        [CrContainerShadowSide.TOP];
    this.sides_.forEach(side => {
      // The element holding the drop shadow effect to be shown.
      const shadow = document.createElement('div');
      shadow.id = `cr-container-shadow-${side}`;
      shadow.classList.add('cr-container-shadow');
      this.dropShadows_.set(side, shadow);
      this.intersectionProbes_.set(side, document.createElement('div'));
    });

    this.$.container.parentNode.insertBefore(
        this.dropShadows_.get(CrContainerShadowSide.TOP), this.$.container);
    this.$.container.prepend(
        this.intersectionProbes_.get(CrContainerShadowSide.TOP));

    if (hasBottomShadow) {
      this.$.container.parentNode.insertBefore(
          this.dropShadows_.get(CrContainerShadowSide.BOTTOM),
          this.$.container.nextSibling);
      this.$.container.append(
          this.intersectionProbes_.get(CrContainerShadowSide.BOTTOM));
    }

    this.enableShadowBehavior(true);
  },

  /** @override */
  detached() {
    this.enableShadowBehavior(false);
  },

  /**
   * @return {!IntersectionObserver}
   * @private
   */
  getIntersectionObserver_() {
    const callback = entries => {
      // In some rare cases, there could be more than one entry per observed
      // element, in which case the last entry's result stands.
      for (const entry of entries) {
        const target = entry.target;
        this.sides_.forEach(side => {
          if (target === this.intersectionProbes_.get(side)) {
            this.dropShadows_.get(side).classList.toggle(
                'has-shadow', entry.intersectionRatio === 0);
          }
        });
      }
    };
    return new IntersectionObserver(
        callback,
        /** @type {IntersectionObserverInit} */ ({
          root: this.$.container,
          threshold: 0,
        }));
  },

  /**
   * @param {boolean} enable Whether to enable the behavior or disable it. This
   *     function does nothing if the behavior is already in the requested
   *     state.
   */
  enableShadowBehavior(enable) {
    // Behavior is already enabled/disabled. Return early.
    if (enable === !!this.intersectionObserver_) {
      return;
    }

    if (!enable) {
      this.intersectionObserver_.disconnect();
      this.intersectionObserver_ = null;
      return;
    }

    this.intersectionObserver_ = this.getIntersectionObserver_();

    // Need to register the observer within a setTimeout() callback, otherwise
    // the drop shadow flashes once on startup, because of the DOM modifications
    // earlier in this function causing a relayout.
    window.setTimeout(() => {
      if (this.intersectionObserver_) {  // In case this is already detached.
        this.intersectionProbes_.forEach(probe => {
          this.intersectionObserver_.observe(probe);
        });
      }
    });
  },

  /**
   * Shows the shadows. The shadow behavior must be disabled before calling this
   * method, otherwise the intersection observer might show the shadows again.
   */
  showDropShadows() {
    assert(!this.intersectionObserver_);
    assert(this.sides_);
    for (const side of this.sides_) {
      this.dropShadows_.get(side).classList.toggle('has-shadow', true);
    }
  },
};

/** @interface */
export class CrContainerShadowBehaviorInterface {
  /**
   * @param {boolean} enable
   */
  enableShadowBehavior(enable) {}

  showDropShadows() {}
}
