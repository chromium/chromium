/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * Contains common animations used in the main NTP page and its iframes.
 */
const animations = {};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
animations.CLASSES = {
  RIPPLE: 'ripple',
  RIPPLE_CONTAINER: 'ripple-container',
  RIPPLE_EFFECT_MASK: 'ripple-effect-mask',
  RIPPLE_EFFECT: 'ripple-effect',
};

/**
 * The duration of the ripple animation.
 * @type {number}
 * @const
 */
animations.RIPPLE_DURATION_MS = 800;

/**
 * The max size of the ripple animation.
 * @type {number}
 * @const
 */
animations.RIPPLE_MAX_RADIUS_PX = 300;

/**
 * Enables ripple animations for elements with CLASSES.RIPPLE. The target
 * element must have position relative or absolute.
 */
animations.addRippleAnimations = function() {
  const ripple = (event) => {
    const target = event.target;
    const rect = target.getBoundingClientRect();
    const x = Math.round(event.clientX - rect.left);
    const y = Math.round(event.clientY - rect.top);

    // Calculate radius
    const corners = [
      {x: 0, y: 0},
      {x: rect.width, y: 0},
      {x: 0, y: rect.height},
      {x: rect.width, y: rect.height},
    ];
    const distance = (x1, y1, x2, y2) => {
      const xDelta = x1 - x2;
      const yDelta = y1 - y2;
      return Math.sqrt(xDelta * xDelta + yDelta * yDelta);
    };
    const cornerDistances = corners.map(function(corner) {
      return Math.round(distance(x, y, corner.x, corner.y));
    });
    const radius = Math.min(
        animations.RIPPLE_MAX_RADIUS_PX, Math.max.apply(Math, cornerDistances));

    const ripple = document.createElement('div');
    const rippleMask = document.createElement('div');
    const rippleContainer = document.createElement('div');
    ripple.classList.add(animations.CLASSES.RIPPLE_EFFECT);
    rippleMask.classList.add(animations.CLASSES.RIPPLE_EFFECT_MASK);
    rippleContainer.classList.add(animations.CLASSES.RIPPLE_CONTAINER);
    rippleMask.appendChild(ripple);
    rippleContainer.appendChild(rippleMask);
    target.appendChild(rippleContainer);
    // Ripple start location
    ripple.style.marginLeft = x + 'px';
    ripple.style.marginTop = y + 'px';

    rippleMask.style.width = target.offsetWidth + 'px';
    rippleMask.style.height = target.offsetHeight + 'px';
    rippleMask.style.borderRadius =
        window.getComputedStyle(target).borderRadius;

    // Start transition/ripple
    ripple.style.width = radius * 2 + 'px';
    ripple.style.height = radius * 2 + 'px';
    ripple.style.marginLeft = x - radius + 'px';
    ripple.style.marginTop = y - radius + 'px';
    ripple.style.backgroundColor = 'rgba(0, 0, 0, 0)';

    window.setTimeout(function() {
      ripple.remove();
      rippleMask.remove();
      rippleContainer.remove();
    }, animations.RIPPLE_DURATION_MS);
  };

  const rippleElements =
      document.querySelectorAll('.' + animations.CLASSES.RIPPLE);
  for (let i = 0; i < rippleElements.length; i++) {
    rippleElements[i].addEventListener('mousedown', ripple);
  }
};
