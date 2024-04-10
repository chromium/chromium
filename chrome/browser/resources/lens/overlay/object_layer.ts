// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {LensPageCallbackRouter} from './lens.mojom-webui.js';
import {getTemplate} from './object_layer.html.js';
import type {OverlayObject} from './overlay_object.mojom-webui.js';
import type {GestureEvent} from './selection_utils.js';

// Takes the value between 0-1 and returns a string in the from '__%';
function toPercent(value: number): string {
  return `${value * 100}%`;
}

// Returns true if the object has a valid bounding box and is renderable by the
// ObjectLayer.
function isObjectRenderable(object: OverlayObject): boolean {
  // For an object to be renderable, it must have a bounding box with normalized
  // coordinates.
  // TODO(b/330183480): Add rendering for IMAGE CoordinateType
  const objectBoundingBox = object.geometry?.boundingBox;
  if (!objectBoundingBox) {
    return false;
  }

  return objectBoundingBox.coordinateType ===
      CenterRotatedBox_CoordinateType.kNormalized;
}

// Orders objects with larger areas before objects with smaller areas.
function compareArea(object1: OverlayObject, object2: OverlayObject): number {
  assert(object1.geometry);
  assert(object2.geometry);
  return object2.geometry.boundingBox.box.width *
      object2.geometry.boundingBox.box.height -
      object1.geometry.boundingBox.box.width *
      object1.geometry.boundingBox.box.height;
}

export interface ObjectLayerElement {
  $: {
    objectsContainer: DomRepeat,
  };
}

/*
 * Element responsible for highlighting and selection text.
 */
export class ObjectLayerElement extends PolymerElement {
  static get is() {
    return 'lens-object-layer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      renderedObjects: {
        type: Array,
        value: () => [],
      },
    };
  }

  // The objects rendered in this layer.
  private renderedObjects: OverlayObject[];

  private readonly router: LensPageCallbackRouter =
      BrowserProxyImpl.getInstance().callbackRouter;
  private objectsReceivedListenerId: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    // Set up listener to receive objects from C++.
    this.objectsReceivedListenerId = this.router.objectsReceived.addListener(
        this.onObjectsReceived.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // Remove listener to receive objects from C++.
    assert(this.objectsReceivedListenerId);
    this.router.removeListener(this.objectsReceivedListenerId);
    this.objectsReceivedListenerId = null;
  }

  handleUpGesture(event: GestureEvent): boolean {
    const objectIndex = this.objectIndexFromPoint(event.clientX, event.clientY);
    // Ignore if the click is not on an object.
    if (objectIndex === null) {
      return false;
    }

    BrowserProxyImpl.getInstance().handler.issueLensRequest(
        this.renderedObjects[objectIndex].geometry!.boundingBox);
    return true;
  }

  private onObjectsReceived(objects: OverlayObject[]) {
    // Sort objects by descending bounding box areas so that smaller objects
    // are rendered over, and take priority over, larger objects.
    this.renderedObjects =
        objects.filter(o => isObjectRenderable(o)).sort(compareArea);
  }

  /** @return The CSS styles string for the given object. */
  private getObjectStyle(object: OverlayObject): string {
    // Objects without bounding boxes are filtered out, so guaranteed that
    // geometry is not null.
    const objectBoundingBox = object.geometry!.boundingBox;

    // TODO(b/330183480): Currently, we are assuming that object
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    if (objectBoundingBox.coordinateType !==
        CenterRotatedBox_CoordinateType.kNormalized) {
      return '';
    }

    // Put into an array instead of a long string to keep this code readable.
    const styles: string[] = [
      `width: ${toPercent(objectBoundingBox.box.width)}`,
      `height: ${toPercent(objectBoundingBox.box.height)}`,
      `top: ${
          toPercent(
              objectBoundingBox.box.y - (objectBoundingBox.box.height / 2))}`,
      `left: ${
          toPercent(
              objectBoundingBox.box.x - (objectBoundingBox.box.width / 2))}`,
      `transform: rotate(${objectBoundingBox.rotation}rad)`,
    ];
    return styles.join(';');
  }

  /**
   * @return Returns the index in renderedObjects of the object at the given
   *     point. Returns null if no object is at the given point.
   */
  private objectIndexFromPoint(x: number, y: number): number|null {
    // Find the top-most element at the clicked point that is an object.
    // elementFromPoint() may select non-object elements that have a higher
    // z-index.
    const elementsAtPoint = this.shadowRoot!.elementsFromPoint(x, y);
    for (const element of elementsAtPoint) {
      if (!(element instanceof HTMLElement)) {
        continue;
      }
      const index = this.$.objectsContainer.indexForElement(element);
      if (index !== null) {
        return index;
      }
    }
    return null;
  }

  // Testing method to get the objects on the page.
  getObjectNodesForTesting() {
    return this.shadowRoot!.querySelectorAll<HTMLElement>('.object');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-object-layer': ObjectLayerElement;
  }
}

customElements.define(ObjectLayerElement.is, ObjectLayerElement);
