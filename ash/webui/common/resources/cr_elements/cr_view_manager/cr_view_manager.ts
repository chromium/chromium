// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from
// ui/webui/resources/cr_elements/cr_view_manager/cr_view_manager.ts

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrLazyRenderElement} from '../cr_lazy_render/cr_lazy_render.js';

import {getTemplate} from './cr_view_manager.html.js';

function getEffectiveView<T extends HTMLElement>(
    element: CrLazyRenderElement<T>|T): HTMLElement {
  return element.matches('cr-lazy-render') ?
      (element as CrLazyRenderElement<T>).get() :
      element;
}

function dispatchCustomEvent(element: Element, eventType: string) {
  element.dispatchEvent(
      new CustomEvent(eventType, {bubbles: true, composed: true}));
}

const viewAnimations: Map<string, (element: Element) => Promise<Animation>> =
    new Map();
viewAnimations.set('fade-in', element => {
  const animation = element.animate([{opacity: 0}, {opacity: 1}], {
    duration: 180,
    easing: 'ease-in-out',
    iterations: 1,
  });

  return animation.finished;
});
viewAnimations.set('fade-out', element => {
  const animation = element.animate([{opacity: 1}, {opacity: 0}], {
    duration: 180,
    easing: 'ease-in-out',
    iterations: 1,
  });

  return animation.finished;
});
viewAnimations.set('slide-in-fade-in-ltr', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(-8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1},
      ],
      {
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      });

  return animation.finished;
});
viewAnimations.set('slide-in-fade-in-rtl', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1},
      ],
      {
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      });

  return animation.finished;
});

export class CrViewManagerElement extends PolymerElement {
  static get is() {
    return 'cr-view-manager';
  }

  static get template() {
    return getTemplate();
  }

  private exit_(element: HTMLElement, animation: string): Promise<void> {
    const animationFunction = viewAnimations.get(animation);
    element.classList.remove('active');
    element.classList.add('closing');
    dispatchCustomEvent(element, 'view-exit-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
      return Promise.resolve();
    }
    return animationFunction(element).then(() => {
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
    });
  }

  private enter_(view: HTMLElement, animation: string): Promise<void> {
    const animationFunction = viewAnimations.get(animation);
    const effectiveView = getEffectiveView(view);
    effectiveView.classList.add('active');
    dispatchCustomEvent(effectiveView, 'view-enter-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
      return Promise.resolve();
    }
    return animationFunction(effectiveView).then(() => {
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
    });
  }

  switchView(
      newViewId: string, enterAnimation?: string,
      exitAnimation?: string): Promise<void> {
    const previousView = this.querySelector<HTMLElement>('.active');
    const newView = this.querySelector<HTMLElement>('#' + newViewId);
    assert(!!newView);

    const promises = [];
    if (previousView) {
      promises.push(this.exit_(previousView, exitAnimation || 'fade-out'));
      promises.push(this.enter_(newView, enterAnimation || 'fade-in'));
    } else {
      promises.push(this.enter_(newView, 'no-animation'));
    }

    return Promise.all(promises).then(() => {});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-view-manager': CrViewManagerElement;
  }
}

customElements.define(CrViewManagerElement.is, CrViewManagerElement);
