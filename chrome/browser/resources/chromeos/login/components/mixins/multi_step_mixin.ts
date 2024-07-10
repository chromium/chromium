// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '//resources/js/assert.js';
import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeBaseMixin, OobeBaseMixinInterface} from './oobe_base_mixin.js';

/**
 * @fileoverview
 * 'MultiStepMixin' is a mixin that simplifies defining and handling
 * elements that have different UI depending on the state, e.g. screens that
 * have several steps.
 *
 * This mixin controls elements in local DOM marked with for-step attribute.
 * Attribute value should be a comma-separated list of steps for which element
 * should be visible.
 *
 * Example usage (note that element can be shown for multiple steps as well as
 * multiple elements can be show for same step):
 *
 * <dom-module id="my-element">
 *     <template>
 *       ...
 *       <loading-indicator for-step="loading"></loading-indicator>
 *       <some-dialog for-step="action">
 *         ...
 *       </some-dialog>
 *       <error-dialog for-step="error-1,error-2">
 *         <error-message for-step="error-1">...</error-message>
 *         <error-message for-step="error-2">...</error-message>
 *         <illustration for-step="error-2">...</illustration>
 *         ...
 *       </error-dialog>
 *     </template>
 * </dom-module>
 */

type Constructor<T> = new (...args: any[]) => T;

export const MultiStepMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MultiStepMixinInterface> => {
      const superClassBase = OobeBaseMixin(superClass);
      class MultiStepMixinInternal extends superClassBase implements
          MultiStepMixinInterface {
        static get properties(): PolymerElementProperties {
          return {
            uiStep: {
              type: String,
              value: '',
            },
          };
        }

        uiStep: string;

        /*
         * List of UI states, must be replaced by implementing component.
         * Returns an enum-style map.
         */
        get UI_STEPS(): Record<string, string> {
          return {};
        }

        /*
         * Map from step name to elements that should be visible on that step.
         * Used for performance optimization.
         */
        private stepElements: Record<string, HTMLElement[]> = {};

        /*
         * Whether the element is shown (Between onBeforeShow and onBeforeHide
         * calls).
         */
        private shown: boolean = false;

        /*
         * Method that lists all possible steps for current element.
         * Default implementation uses value UI_STEPS of type enum-style object.
         * Elements that delegate part of the states to the child element might
         * need to override this method.
         */
        listSteps(): string[] {
          return Object.values(this.UI_STEPS);
        }

        /*
         * Element must override this method to return name of the initial
         * step.
         */
        // eslint-disable-next-line @typescript-eslint/naming-convention
        defaultUIStep(): string {
          console.error('defaultUIStep() must be overridden in screen');
          return 'Invalid';
        }

        override ready(): void {
          super.ready();
          // Add marker class for quickly finding children with same mixin.
          this.classList.add('multi-state-element');
          this.refreshStepBindings();
        }

        override onBeforeShow(...data: any[]): void {
          super.onBeforeShow(data);

          this.shown = true;
          // Only set uiStep to defaultUIStep if it is not set yet.
          if (!this.uiStep) {
            this.setUIStep(this.defaultUIStep());
          } else {
            this.showUiStep(this.uiStep);
          }
        }

        override onBeforeHide(): void {
          super.onBeforeHide();

          if (this.uiStep) {
            this.hideUIStep(this.uiStep);
          }
          this.shown = false;
        }

        /**
         * Returns default event target element.
         */
        override get defaultControl(): HTMLElement|null {
          return this.stepElements[this.defaultUIStep()][0];
        }

        /*
         * Method that applies a function to all elements of a step (default to
         * current step).
         */
        applyToStepElements(
            func: (arg0: HTMLElement) => void,
            step: string = this.uiStep): void {
          for (const element of this.stepElements[step] || []) {
            func(element);
          }
        }

        // eslint-disable-next-line @typescript-eslint/naming-convention
        setUIStep(step: string): void {
          if (this.uiStep) {
            if (this.uiStep === step) {
              return;
            }
            this.hideUIStep(this.uiStep);
          }
          this.uiStep = step;
          this.shadowRoot?.host?.setAttribute('multistep', step);
          this.showUiStep(this.uiStep);
        }

        private showUiStep(step: string): void {
          if (!this.shown) {
            // Will execute from onBeforeShow.
            return;
          }
          for (const element of this.stepElements[step] || []) {
            if ('onBeforeShow' in element &&
                typeof element.onBeforeShow === 'function') {
              element.onBeforeShow();
            }
            element.hidden = false;
            // Trigger show() if element is an oobe-dialog
            if ('show' in element && typeof element.show === 'function') {
              element.show();
            }
          }
        }

        // eslint-disable-next-line @typescript-eslint/naming-convention
        private hideUIStep(step: string): void {
          for (const element of this.stepElements[step] || []) {
            if ('onBeforeHide' in element &&
                typeof element.onBeforeHide === 'function') {
              element.onBeforeHide();
            }
            element.hidden = true;
          }
        }

        /*
         * Fills stepElements map by looking up child elements with for-step
         * attribute
         */
        private refreshStepBindings(): void {
          this.stepElements = {};
          const matches = this.shadowRoot?.querySelectorAll('[for-step]') || [];
          for (const child of matches) {
            assertInstanceof(
                child, HTMLElement,
                'Each element assigned to a multi step has to be of type ' +
                    'HTMLElement');
            const stepsList = child.getAttribute('for-step')?.split(',') || [];
            for (const stepChunk of stepsList) {
              const step = stepChunk.trim();
              const list = this.stepElements[step] || [];
              list.push(child);
              this.stepElements[step] = list;
            }
            child.hidden = true;
          }
        }
      }

      return MultiStepMixinInternal;
    });

export interface MultiStepMixinInterface extends OobeBaseMixinInterface {
  get uiStep(): string;
  get UI_STEPS(): Record<string, string>;
  listSteps(): string[];
  defaultUIStep(): string;
  onBeforeShow(...data: any[]): void;
  onBeforeHide(): void;
  get defaultControl(): HTMLElement|null;
  applyToStepElements(func: (arg0: HTMLElement) => void, step?: string): void;
  setUIStep(step: string): void;
}
