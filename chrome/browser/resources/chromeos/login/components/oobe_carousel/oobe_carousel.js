// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-carousel',

  properties: {
    /**
     * Current slide index.
     */
    slideIndex: {
      type: Number,
      value: 0,
      observer: 'onSlideIndexChanged_',
    },

    /**
     * Controls if slides will be rotated automatically.
     * Note: This feature is not a11y friendly and will cause bugs when
     * ChromeVox is turned on. There is a need to observe ChromeVox and stop
     * auto transition when ChromeVox is turned on.
     */
    autoTransition: {
      type: Boolean,
      value: false,
      observer: 'restartAutoTransition_',
    },

    /**
     * Number of seconds each slide should remain for.
     */
    slideDurationInSeconds: {
      type: Number,
      value: 10,
      observer: 'restartAutoTransition_',
    },

    /**
     * Slide aria-label.
     */
    slideLabel: String,

    /**
     * Selected button aria-label.
     */
    selectedButtonLabel: String,

    /**
     * Unselected button aria-label.
     */
    unselectedButtonLabel: String,
  },

  /**
   * Array for storing number leading up to totalSlides
   * @type {Array<number>}
   */
  dots: [],

  /**
   * Array of slotted slides.
   * @type {Array<HTMLElement>}
   */
  slides: [],

  /**
   * Total number of slides.
   * @type {number}
   */
  totalSlides: 0,

  /**
   * ID of the timer which rotates slides.
   * @type {number|null}
   */
  timerID: null,

  /** @type {function(Event)} */
  removeAnimateToHandler: Function,

  /** @type {function(Event)} */
  removeAnimateFromHandler: Function,

  attached() {
    this.removeAnimateToHandler = this.removeAnimateTo_.bind(this);
    this.removeAnimateFromHandler = this.removeAnimateFrom_.bind(this);
    this.prepareCarousel_();
    this.restartAutoTransition_();
    this.hideNonActiveSlides_();
  },

  /**
   * @private
   * Count slides and create dots. Set a11y label on slides.
   */
  prepareCarousel_() {
    this.slides = this.$.slot.assignedElements();
    this.totalSlides = this.slides.length;
    let array = [];
    for (let i = 0; i < this.totalSlides; ++i) {
      this.slides[i].setAttribute('aria-label', this.getSlideLabel_(i));
      this.slides[i].setAttribute('role', 'group');
      array.push(i);
    }
    this.dots = array;
  },

  /**
   * @private
   * @param {number} index Index of slide.
   * Returns string label for slide.
   */
  getSlideLabel_(index) {
    return loadTimeData.getStringF(
        this.slideLabel, index + 1, this.totalSlides);
  },

  /**
   * @private
   */
  hideNonActiveSlides_() {
    for (let idx = 0; idx < this.totalSlides; ++idx) {
      if (idx != this.slideIndex)
        this.hideSlide(this.slides[idx]);
    }
  },

  /**
   * @private
   * Re-inits timer which rotates slides if |autorotation| is set.
   */
  restartAutoTransition_() {
    this.stopAutoTransition_();
    this.startAutoTransition_();
  },

  /**
   * @private
   * Inits timer which rotates slides if |autoTransition| is set.
   */
  startAutoTransition_() {
    if (this.autoTransition && this.slideDurationInSeconds != null) {
      this.timerID = setInterval(
          this.moveNext.bind(this), (this.slideDurationInSeconds * 1000));
    }
  },

  /**
   * @private
   * Stops timer which rotates slides.
   */
  stopAutoTransition_() {
    if (this.timerID != null) {
      clearInterval(this.timerID);
      this.timerID = null;
    }
  },

  /**
   * @private
   * @param {number} toIndex Index of slide which should be shown.
   * @param {number|undefined} fromIndex Index of slide which should be hidden.
   * Method which moves slides to show active one.
   */
  animateSlides_(toIndex, fromIndex) {
    if (fromIndex == 0 && toIndex == this.totalSlides - 1) {
      this.animateInternal_(toIndex, fromIndex, false);
      return;
    }
    if (fromIndex == this.totalSlides - 1 && toIndex == 0) {
      this.animateInternal_(toIndex, fromIndex, true);
      return;
    }

    if (toIndex < fromIndex) {
      this.animateInternal_(toIndex, fromIndex, false);
    }
    if (toIndex > fromIndex) {
      this.animateInternal_(toIndex, fromIndex, true);
    }
  },

  /**
   * @private
   * @param {number} toIndex Index of slide which should be shown.
   * @param {number|undefined} fromIndex Index of slide which should be hidden.
   * @param {boolean} forward Show forward animation or backward.
   * Method which moves slides to show active one using set direction.
   */
  animateInternal_(toIndex, fromIndex, forward) {
    if (forward) {
      this.animateInternalWithStyles_(
          toIndex, fromIndex, 'forward', 'backward');
    } else {
      this.animateInternalWithStyles_(
          toIndex, fromIndex, 'backward', 'forward');
    }
  },

  /**
   * @private
   * @param {EventTarget|null} slide
   */
  hideSlide(slide) {
    slide.setAttribute('aria-hidden', 'true');
    slide.hidden = true;
  },

  /**
   * @private
   * @param {EventTarget|null} slide
   */
  showSlide(slide) {
    slide.removeAttribute('aria-hidden');
    slide.hidden = false;
  },

  /**
   * @private
   * @param {EventTarget|null} slide
   */
  cleanStyles(slide) {
    slide.classList.remove('animated', 'forward', 'backward', 'hide-slide');
  },

  /**
   * @private
   * @param {Event} event transitionend event.
   */
  removeAnimateTo_(event) {
    const toElement = event.target;

    this.cleanStyles(toElement);
    toElement.removeEventListener('transitionend', this.removeAnimateToHandler);
  },

  /**
   * @private
   * @param {Event} event transitionend event.
   */
  removeAnimateFrom_(event) {
    const fromElement = event.target;

    this.hideSlide(fromElement);
    this.cleanStyles(fromElement);
    fromElement.removeEventListener(
        'transitionend', this.removeAnimateFromHandler);
  },

  /**
   * @private
   * @param {number} fromIndex Index of slide which should be hidden.
   * Clean the state the could be uncertain due to several transitions started
   * one after another without waiting for their end.
   */
  cleanUpState_(fromIndex) {
    for (let idx = 0; idx < this.totalSlides; ++idx) {
      const slide = this.slides[idx];
      slide.removeEventListener('transitionend', this.removeAnimateFromHandler);
      slide.removeEventListener('transitionend', this.removeAnimateToHandler);
      if (idx != fromIndex) {
        this.hideSlide(slide);
      }
      this.cleanStyles(slide);
    }
    this.showSlide(this.slides[fromIndex]);
  },

  /**
   * @private
   * @param {number} toIndex Index of slide which should be shown.
   * @param {number|undefined} fromIndex Index of slide which should be hidden.
   * @param {string} toStyle CSS class to apply on the slide to be shown.
   * @param {string} fromStyle CSS class to apply on the slide to be hidden.
   * @suppress {uselessCode} To avoid error on calling void operator.
   * Animates slides using given CSS class animation.
   */
  animateInternalWithStyles_(toIndex, fromIndex, toStyle, fromStyle) {
    if (fromIndex == null) {
      return;
    }
    this.cleanUpState_(fromIndex);

    const toElement = this.slides[toIndex];
    const fromElement = this.slides[fromIndex];

    toElement.removeAttribute('aria-hidden');
    toElement.removeAttribute('hidden');
    toElement.classList.add(toStyle);
    // Call offsetWidth to apply |toStyle| and render |toElement|.
    void toElement.offsetWidth;

    toElement.classList.add('animated');
    fromElement.classList.add('animated');
    toElement.classList.remove(toStyle);
    fromElement.classList.add(fromStyle);
    fromElement.classList.add('hide-slide');

    toElement.addEventListener('transitionend', this.removeAnimateToHandler);
    fromElement.addEventListener(
        'transitionend', this.removeAnimateFromHandler);
  },

  /**
   * @private
   * @param {Event} e keypress event.
   * On key press function.
   */
  onKeypress_(e) {
    // Space (32) and enter (13) key codes.
    if (e.keyCode == 32 || e.keyCode == 13)
      this.slideIndex = e.model.item;
  },

  /**
   * @private
   * @param {Event} e click event.
   * On dot click function.
   */
  onClick_(e) {
    this.slideIndex = e.model.item;
  },

  /**
   * @private
   * @param {number} index Index of dot.
   * Returns whether a dot is active.
   */
  isActive_(index) {
    return index == this.slideIndex;
  },

  /**
   * @private
   * @param {number} index Index of slide.
   * Returns string label for dot.
   */
  getDotLabel_(index) {
    if (index == this.slideIndex) {
      return loadTimeData.getStringF(
          this.selectedButtonLabel, index + 1, this.totalSlides);
    }
    return loadTimeData.getStringF(
        this.unselectedButtonLabel, index + 1, this.totalSlides);
  },

  /**
   * @private
   * @param {number} toIndex Index of slide which should be shown.
   * @param {number|undefined} fromIndex Index of slide which should be hidden.
   * Observe index change and activates all animation and slides attributes
   * changes.
   */
  onSlideIndexChanged_(toIndex, fromIndex) {
    this.restartAutoTransition_();
    this.animateSlides_(toIndex, fromIndex);
  },

  moveNext() {
    this.slideIndex = (this.slideIndex + 1) % this.totalSlides;
  },

  movePrev() {
    this.slideIndex =
        (this.slideIndex + this.totalSlides - 1) % this.totalSlides;
  },
});
