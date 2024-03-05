// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Simple OOBE polymer element which is used for displaying slides in a
 * carousel. It has fixed height and width which match general width and height
 * oobe-dialog restrictions.
 *
 *  Example (each slide must be within a oobe-slide tag):
 *    <oobe-carousel slide-duration="2">
 *      <div slot="slides">Slide 1</div>
 *      <div slot="slides">Slide 2</div>
 *      <div slot="slides">Slide 3</div>
 *    </oobe-carousel>
 *
 *  Note: This element assumes that load_time_data is included in the enclosing
 *  document level.
 */

import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import './common_styles/oobe_common_styles.css.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeScrollableMixin} from './mixins/oobe_scrollable_mixin.js';

import {getTemplate} from './oobe_carousel.html.js';

const OobeCarouselBase = OobeScrollableMixin(PolymerElement);

export class OobeCarousel extends OobeCarouselBase {
  static get is() {
    return 'oobe-carousel' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Current slide index.
       */
      slideIndex: {
        type: Number,
        value: 0,
        observer: 'onSlideIndexChanged',
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
        observer: 'restartAutoTransition',
      },

      /**
       * Number of seconds each slide should remain for.
       */
      slideDurationInSeconds: {
        type: Number,
        value: 8,
        observer: 'restartAutoTransition',
      },

      /**
       * Slide aria-label.
       */
      slideLabel: {
        type: String,
      },

      /**
       * Selected button aria-label.
       */
      selectedButtonLabel: {
        type: String,
      },

      /**
       * Unselected button aria-label.
       */
      unselectedButtonLabel: {
        type: String,
      },
    };
  }

  private slideIndex: number;
  private autoTransition: boolean;
  private slideDurationInSeconds: number;
  private slideLabel: string;
  private selectedButtonLabel: string;
  private unselectedButtonLabel: string;
  private dots: number[];
  private slides: HTMLElement[];
  private totalSlides: number;
  private timerId: number|null;

  constructor() {
    super();

    /**
     * Array for storing number leading up to totalSlides
     * Example: [ 0 1 2 3 ... ]
     */
    this.dots = [];

    /**
     * Array of slotted slides.
     */
    this.slides = [];

    /**
     * Total number of slides.
     */
    this.totalSlides = 0;

    /**
     * ID of the timer which rotates slides.
     */
    this.timerId = null;
  }

  override ready() {
    super.ready();
    this.prepareCarousel();
    this.restartAutoTransition();
    this.hideNonActiveSlides();

    const slidesContainer = this.shadowRoot?.querySelector('#slidesContainer');
    assert(slidesContainer instanceof HTMLDivElement);
    slidesContainer.addEventListener(
        'mouseenter', () => this.stopAutoTransition());
    slidesContainer.addEventListener(
        'mouseleave', () => this.startAutoTransition());

    const scrollContainer = this.shadowRoot?.querySelector('#scrollContainer');
    assert(scrollContainer instanceof HTMLDivElement);
    this.initScrollableObservers(scrollContainer, slidesContainer);
  }

  /**
   * Count slides and create dots. Set a11y label on slides.
   */
  private prepareCarousel(): void {
    const slot = this.shadowRoot?.querySelector('#slot');
    assert(slot instanceof HTMLSlotElement);

    const slotted = slot.assignedElements() as HTMLElement[];
    assert(Array.isArray(slotted) &&
      slotted.every(elem => elem instanceof HTMLElement));
    this.slides = slotted;

    this.totalSlides = this.slides.length;
    this.dots = [...Array(this.totalSlides).keys()];
    for (let i = 0; i < this.totalSlides; ++i) {
      this.slides[i].setAttribute('aria-label', this.getSlideLabel(i));
      this.slides[i].setAttribute('role', 'group');
    }
  }

  /**
   * @param index Index of slide.
   * Returns string label for slide.
   */
  private getSlideLabel(index: number): string {
    return loadTimeData.getStringF(
        this.slideLabel, index + 1, this.totalSlides);
  }

  private hideNonActiveSlides(): void {
    for (let idx = 0; idx < this.totalSlides; ++idx) {
      if (idx !== this.slideIndex) {
        OobeCarousel.hideSlide(this.slides[idx]);
      }
    }
  }

  /**
   * Re-inits timer which rotates slides if |autorotation| is set.
   */
  private restartAutoTransition(): void {
    this.stopAutoTransition();
    this.startAutoTransition();
  }

  /**
   * Inits timer which rotates slides if |autoTransition| is set.
   */
  private startAutoTransition(): void {
    if (this.autoTransition && this.slideDurationInSeconds != null) {
      this.timerId = setInterval(
          () => this.moveNext(), (this.slideDurationInSeconds * 1000));
    }
  }

  /**
   * Stops timer which rotates slides.
   */
  private stopAutoTransition(): void {
    if (this.timerId != null) {
      clearInterval(this.timerId);
      this.timerId = null;
    }
  }

  /**
   * @param Index of slide which should be shown.
   * @param fromIndex Index of slide which should be hidden.
   * Method which moves slides to show active one.
   */
  private animateSlides(toIndex: number, fromIndex: number): void {
    if (fromIndex === 0 && toIndex === this.totalSlides - 1) {
      this.animateInternal(toIndex, fromIndex, false);
      return;
    }
    if (fromIndex === this.totalSlides - 1 && toIndex === 0) {
      this.animateInternal(toIndex, fromIndex, true);
      return;
    }

    if (toIndex < fromIndex) {
      this.animateInternal(toIndex, fromIndex, false);
    }
    if (toIndex > fromIndex) {
      this.animateInternal(toIndex, fromIndex, true);
    }
  }

  /**
   * @param toIndex Index of slide which should be shown.
   * @param fromIndex Index of slide which should be hidden.
   * @param forward Show forward animation or backward.
   * Method which moves slides to show active one using set direction.
   */
  private animateInternal(toIndex: number, fromIndex: number,
      forward: boolean): void {
    if (forward) {
      this.animateInternalWithStyles(
          toIndex, fromIndex, 'forward', 'backward');
    } else {
      this.animateInternalWithStyles(
          toIndex, fromIndex, 'backward', 'forward');
    }
  }

  /**
   * @param slide
   */
  private static hideSlide(slide: HTMLElement): void {
    slide.setAttribute('aria-hidden', 'true');
    slide.hidden = true;
  }

  /**
   * @param slide
   */
  private static showSlide(slide: HTMLElement): void {
    slide.removeAttribute('aria-hidden');
    slide.hidden = false;
  }

  /**
   * @param slide
   */
  private static cleanStyles(slide: HTMLElement): void {
    slide.classList.remove('animated', 'forward', 'backward', 'hide-slide');
  }

  /**
   * @param event transitionend event.
   */
  private static removeAnimateTo(event: Event): void {
    const toElement = event.target;
    assert(toElement instanceof HTMLElement);

    OobeCarousel.cleanStyles(toElement);
    toElement.removeEventListener(
        'transitionend', OobeCarousel.removeAnimateTo);
  }

  /**
   * @param event transitionend event.
   */
  private static removeAnimateFrom(event: Event): void {
    const fromElement = event.target;
    assert(fromElement instanceof HTMLElement);

    OobeCarousel.hideSlide(fromElement);
    OobeCarousel.cleanStyles(fromElement);
    fromElement.removeEventListener(
        'transitionend', OobeCarousel.removeAnimateFrom);
  }

  /**
   * @param fromIndex Index of slide which should be hidden.
   * Clean the state the could be uncertain due to several transitions started
   * one after another without waiting for their end.
   */
  private cleanUpState(fromIndex: number): void {
    for (let idx = 0; idx < this.totalSlides; ++idx) {
      const slide = this.slides[idx];
      slide.removeEventListener(
          'transitionend', OobeCarousel.removeAnimateFrom);
      slide.removeEventListener('transitionend', OobeCarousel.removeAnimateTo);
      if (idx !== fromIndex) {
        OobeCarousel.hideSlide(slide);
      }
      OobeCarousel.cleanStyles(slide);
    }
    OobeCarousel.showSlide(this.slides[fromIndex]);
  }

  /**
   * @param toIndex Index of slide which should be shown.
   * @param fromIndex Index of slide which should be hidden.
   * @param toStyle CSS class to apply on the slide to be shown.
   * @param fromStyle CSS class to apply on the slide to be hidden.
   * Animates slides using given CSS class animation.
   */
  private animateInternalWithStyles(toIndex: number, fromIndex: number,
      toStyle: string, fromStyle: string): void {
    if (fromIndex == null) {
      return;
    }
    this.cleanUpState(fromIndex);

    const toElement = this.slides[toIndex];
    const fromElement = this.slides[fromIndex];

    toElement.removeAttribute('aria-hidden');
    toElement.removeAttribute('hidden');
    toElement.classList.add(toStyle);
    // Call offsetWidth to apply |toStyle| and render |toElement|.
    void toElement.offsetWidth;

    // Trigger oobe_scroll_behavior to update scroll indicators
    // as scrollbar can appear immediately on unhiding a tall slide
    this.applyScrollClassTags();

    toElement.classList.add('animated');
    fromElement.classList.add('animated');
    toElement.classList.remove(toStyle);
    fromElement.classList.add(fromStyle);
    fromElement.classList.add('hide-slide');

    toElement.addEventListener('transitionend', OobeCarousel.removeAnimateTo);
    fromElement.addEventListener(
        'transitionend', OobeCarousel.removeAnimateFrom);
    // Trigger oobe_scroll_behavior to update scroll indicators
    // in case the transition was from a tall to a narrow slide
    fromElement.addEventListener(
        'transitionend', () => this.applyScrollClassTags(), {once: true});
  }

  private onKeypress(e: DomRepeatEvent< number, KeyboardEvent >): void {
    if (e.key === 'Space' || e.key === 'Enter') {
      this.slideIndex = e.model.item;
    }
  }

  private onClick(e: DomRepeatEvent< number, MouseEvent >): void {
    this.slideIndex = e.model.item;
    // Set a timer to remove the focus ring.
    setTimeout(
      (element: HTMLElement) => element.blur(),
      this.slideDurationInSeconds * 1000,
      e.currentTarget);
  }

  /**
   * Returns whether a dot is active.
   */
  private isActive(index: number): boolean {
    return index === this.slideIndex;
  }

  /**
   * Returns string label for dot.
   */
  private getDotLabel(index: number): string {
    if (index === this.slideIndex) {
      return loadTimeData.getStringF(
          this.selectedButtonLabel, index + 1, this.totalSlides);
    }
    return loadTimeData.getStringF(
        this.unselectedButtonLabel, index + 1, this.totalSlides);
  }

  /**
   * @private
   * @param {number} toIndex Index of slide which should be shown.
   * @param {number} fromIndex Index of slide which should be hidden.
   * Observe index change and activates all animation and slides attributes
   * changes.
   */
  private onSlideIndexChanged(toIndex: number, fromIndex: number): void {
    this.restartAutoTransition();
    this.animateSlides(toIndex, fromIndex);
  }

  private moveNext(): void {
    this.slideIndex = (this.slideIndex + 1) % this.totalSlides;
  }

  private movePrev(): void {
    this.slideIndex =
        (this.slideIndex + this.totalSlides - 1) % this.totalSlides;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeCarousel.is]: OobeCarousel;
  }
}

customElements.define(OobeCarousel.is, OobeCarousel);
