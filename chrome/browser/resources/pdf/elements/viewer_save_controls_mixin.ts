// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {CrLitElement, PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type Constructor<T> = new (...args: any[]) => T;
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

export const ViewerSaveControlsMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<ViewerSaveControlsMixinInterface> => {
  class ViewerSaveControlsMixin extends superClass implements
      ViewerSaveControlsMixinInterface {
    static get properties() {
      return {
        hasEdits: {type: Boolean},
        hasEnteredAnnotationMode: {type: Boolean},
        // <if expr="enable_pdf_ink2">
        hasInk2Edits: {type: Boolean},
        // </if>
        isFormFieldFocused: {type: Boolean},
      };
    }

    accessor hasEdits: boolean = false;
    accessor hasEnteredAnnotationMode: boolean = false;
    // <if expr="enable_pdf_ink2">
    accessor hasInk2Edits: boolean = false;
    // </if>
    accessor isFormFieldFocused: boolean = false;
    private waitForFormFocusChange_: PromiseResolver<boolean>|null = null;


    override updated(changedProperties: PropertyValues<this>) {
      super.updated(changedProperties);

      if (changedProperties.has('isFormFieldFocused') &&
          this.waitForFormFocusChange_ !== null) {
        // Resolving the promise in updated(), since this can trigger
        // showSaveMenu() which accesses the element's DOM.
        this.waitForFormFocusChange_.resolve(this.hasEdits);
        this.waitForFormFocusChange_ = null;
      }
    }

    /**
     * @return Promise that resolves with true if the PDF has edits and/or
     *     annotations, and false otherwise.
     */
    private waitForEdits_(): Promise<boolean> {
      if (this.hasEditsToSave_()) {
        return Promise.resolve(true);
      }
      if (!this.isFormFieldFocused) {
        return Promise.resolve(false);
      }
      this.waitForFormFocusChange_ = new PromiseResolver();
      return this.waitForFormFocusChange_.promise;
    }

    /**
     * Subclasses should override this method to return the appropriate
     * CrActionMenuElement.
     */
    getMenu(): CrActionMenuElement {
      assertNotReached();
    }

    /**
     * Subclasses should override this method to return the appropriate
     * CrIconButtonElement.
     */
    getSaveButton(): CrIconButtonElement {
      assertNotReached();
    }

    /**
     * Subclasses should override this method to return the appropriate save
     * event type.
     */
    getSaveEventType(): string {
      assertNotReached();
    }

    onSaveClick() {
      this.waitForEdits_().then(hasEdits => {
        if (this.shouldShowSaveMenuOnSaveClick(hasEdits)) {
          this.showSaveMenu_();
        } else {
          this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
        }
      });
    }

    onSaveEditedClick() {
      this.getMenu().close();

      // <if expr="enable_pdf_ink2">
      // Only save as annotation when there are edits.
      if (this.hasInk2Edits) {
        this.dispatchSaveEvent_(SaveRequestType.ANNOTATION);
        return;
      }
      // </if>

      this.dispatchSaveEvent_(
          this.hasEnteredAnnotationMode ? SaveRequestType.ANNOTATION :
                                          SaveRequestType.EDITED);
    }

    onSaveOriginalClick() {
      this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
      this.getMenu().close();
    }

    /**
     * Subclasses can override this method to control whether the save menu
     * should be shown.
     */
    shouldShowSaveMenuOnSaveClick(hasEdits: boolean): boolean {
      return hasEdits;
    }

    private dispatchSaveEvent_(type: SaveRequestType) {
      this.fire(this.getSaveEventType(), type);
    }

    private hasEditsToSave_(): boolean {
      // <if expr="enable_pdf_ink2">
      return this.hasEnteredAnnotationMode || this.hasEdits ||
          this.hasInk2Edits;
      // </if>
      // <if expr="not enable_pdf_ink2">
      return this.hasEnteredAnnotationMode || this.hasEdits;
      // </if>
    }

    private showSaveMenu_() {
      this.getMenu().showAt(this.getSaveButton(), {
        anchorAlignmentX: AnchorAlignment.CENTER,
      });
      // For tests
      this.dispatchEvent(new CustomEvent(
          'save-menu-shown-for-testing', {bubbles: true, composed: true}));
    }

    /**
     * @return The value for the aria-haspopup attribute for the button.
     */
    getAriaHasPopup(): string {
      return this.hasEditsToSave_() ? 'menu' : 'false';
    }
  }
  return ViewerSaveControlsMixin;
};

export interface ViewerSaveControlsMixinInterface {
  hasEdits: boolean;
  hasEnteredAnnotationMode: boolean;
  isFormFieldFocused: boolean;
  getAriaHasPopup(): string;
  getMenu(): CrActionMenuElement;
  getSaveButton(): CrIconButtonElement;
  getSaveEventType(): string;
  onSaveClick(): void;
  onSaveEditedClick(): void;
  onSaveOriginalClick(): void;
  shouldShowSaveMenuOnSaveClick(hasEdits: boolean): boolean;
}
