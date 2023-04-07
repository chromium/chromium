// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Contains currently active progress bar animation.
 * @type {ProgressBarAnimation}
 */
let activeProgressAnimation = null;

/**
 * Provides animation for set of div elements that looks similar to paper
 * progress animation. This class performs animation with the fixed rate that
 * can save CPU resources on low-end machines.
 * TODO(crbug.com/752349): Use paper progress animation.
 */
class ProgressBarAnimation {
  /**
   * @param {Element} progressContainer The root div element for the two child
   *     div elements that are used in animation.
   */
  constructor(progressContainer) {
    this.primaryProgress_ =
        progressContainer.querySelector('.progress-primary');
    this.secondaryProgress_ =
        progressContainer.querySelector('.progress-secondary');
    this.updateInterval_ = null;
    this.animationDurationMs_ = 2000;
    this.fps_ = 10;
  }

  /**
   * Called periodically to perform animation update.
   */
  update_() {
    // Calculate animation time in range 0..1.
    const currentAnimationRatio =
        ((new Date().getTime() - this.startTime_) / this.animationDurationMs_) %
        1;
    // Ranges and constants are taken from paper progress implementation.
    // Animate the primary progress.
    if (currentAnimationRatio <= 0.5) {
      const translate = -100 + 200 * currentAnimationRatio;
      this.primaryProgress_.style.transform =
          'scaleX(1) translateX(' + translate + '%)';
    } else if (currentAnimationRatio <= 0.75) {
      this.primaryProgress_.style.transform = 'scaleX(1) translateX(0%)';
    } else {
      const scale = 4.0 * (1.0 - currentAnimationRatio);
      this.primaryProgress_.style.transform =
          'scaleX(' + scale + ') translateX(0%)';
    }

    // Animate the secondary progress.
    if (currentAnimationRatio < 0.3) {
      this.secondaryProgress_.style.transform =
          'scaleX(0.75) translateX(-125%)';
    } else if (currentAnimationRatio < 0.9) {
      const translate = -125.0 + 250.0 * (currentAnimationRatio - 0.3) / 0.6;
      this.secondaryProgress_.style.transform =
          'scaleX(0.75) translateX(' + translate + '%)';
    } else {
      this.secondaryProgress_.style.transform = 'scaleX(0.75) translateX(125%)';
    }
  }

  /**
   * Starts animation.
   */
  start() {
    this.startTime_ = new Date().getTime();
    this.stop();
    this.updateInterval_ = setInterval(() => this.update_(), 1000 / this.fps_);
  }

  /**
   * Stops animation.
   */
  stop() {
    if (!this.updateInterval_) {
      return;
    }
    clearInterval(this.updateInterval_);
    this.updateInterval_ = null;
  }
}

/**
 * Stops current progress bar animation if it exists.
 */
function stopProgressAnimation() {
  if (!activeProgressAnimation) {
    return;
  }

  activeProgressAnimation.stop();
  activeProgressAnimation = null;
}

/**
 * Starts new progress bar animation and optionally stops previous one.
 * @param {string} pageDivId Page id that contains progress bar to animate.
 */
function startProgressAnimation(pageDivId) {
  stopProgressAnimation();

  const page = $(pageDivId);
  activeProgressAnimation =
      new ProgressBarAnimation(page.querySelector('.progress-container'));
  activeProgressAnimation.start();
}
