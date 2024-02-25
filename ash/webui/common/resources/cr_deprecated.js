// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Leftover "cr." library functions to support some legacy
 * JavaScript users. Do not use this file in new code.
 */

import {assertNotReached} from '//resources/ash/common/assert.js';

/**
 * Dispatches a simple event on an event target.
 * @param {!EventTarget} target The event target to dispatch the event on.
 * @param {string} type The type of the event.
 * @param {boolean=} bubbles Whether the event bubbles or not.
 * @param {boolean=} cancelable Whether the default action of the event
 *     can be prevented. Default is true.
 * @return {boolean} If any of the listeners called {@code preventDefault}
 *     during the dispatch this will return false.
 */
export function dispatchSimpleEvent(target, type, bubbles, cancelable) {
  const e = new Event(
      type,
      {bubbles: bubbles, cancelable: cancelable === undefined || cancelable});
  return target.dispatchEvent(e);
}

/**
 * Fires a property change event on the target.
 * @param {!EventTarget} target The target to dispatch the event on.
 * @param {string} propertyName The name of the property that changed.
 * @param {*} newValue The new value for the property.
 * @param {*} oldValue The old value for the property.
 */
export function dispatchPropertyChange(
    target, propertyName, newValue, oldValue) {
  const e = new Event(propertyName + 'Change');
  e.propertyName = propertyName;
  e.newValue = newValue;
  e.oldValue = oldValue;
  target.dispatchEvent(e);
}

/**
 * Converts a camelCase javascript property name to a hyphenated-lower-case
 * attribute name.
 * @param {string} jsName The javascript camelCase property name.
 * @return {string} The equivalent hyphenated-lower-case attribute name.
 */
function getAttributeName(jsName) {
  return jsName.replace(/([A-Z])/g, '-$1').toLowerCase();
}

/**
 * The kind of property to define in {@code getPropertyDescriptor}.
 * @enum {string}
 * @const
 */
export const PropertyKind = {
  /**
   * Plain old JS property where the backing data is stored as a "private"
   * field on the object.
   * Use for properties of any type. Type will not be checked.
   */
  JS: 'js',

  /**
   * The property backing data is stored as an attribute on an element.
   * Use only for properties of type {string}.
   */
  ATTR: 'attr',

  /**
   * The property backing data is stored as an attribute on an element. If the
   * element has the attribute then the value is true.
   * Use only for properties of type {boolean}.
   */
  BOOL_ATTR: 'boolAttr',
};

/**
 * Helper function for getPropertyDescriptor that returns the getter to use for
 * the property.
 * @param {string} name The name of the property.
 * @param {PropertyKind} kind The kind of the property.
 * @return {function():*} The getter for the property.
 */
function getGetter(name, kind) {
  let attributeName;
  switch (kind) {
    case PropertyKind.JS:
      const privateName = name + '_';
      return function() {
        return this[privateName];
      };
    case PropertyKind.ATTR:
      attributeName = getAttributeName(name);
      return function() {
        return this.getAttribute(attributeName);
      };
    case PropertyKind.BOOL_ATTR:
      attributeName = getAttributeName(name);
      return function() {
        return this.hasAttribute(attributeName);
      };
  }

  assertNotReached();
}

/**
 * Helper function for getPropertyDescriptor that returns the setter of the
 * right kind.
 * @param {string} name The name of the property we are defining the setter
 *     for.
 * @param {PropertyKind} kind The kind of property we are getting the
 *     setter for.
 * @param {function(*, *):void=} setHook A function to run after the
 *     property is set, but before the propertyChange event is fired.
 * @return {function(*):void} The function to use as a setter.
 */
function getSetter(name, kind, setHook) {
  let attributeName;
  switch (kind) {
    case PropertyKind.JS:
      const privateName = name + '_';
      return function(value) {
        const oldValue = this[name];
        if (value !== oldValue) {
          this[privateName] = value;
          if (setHook) {
            setHook.call(this, value, oldValue);
          }
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };

    case PropertyKind.ATTR:
      attributeName = getAttributeName(name);
      return function(value) {
        const oldValue = this[name];
        if (value !== oldValue) {
          if (value === undefined) {
            this.removeAttribute(attributeName);
          } else {
            this.setAttribute(attributeName, value);
          }
          if (setHook) {
            setHook.call(this, value, oldValue);
          }
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };

    case PropertyKind.BOOL_ATTR:
      attributeName = getAttributeName(name);
      return function(value) {
        const oldValue = this[name];
        if (value !== oldValue) {
          if (value) {
            this.setAttribute(attributeName, name);
          } else {
            this.removeAttribute(attributeName);
          }
          if (setHook) {
            setHook.call(this, value, oldValue);
          }
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };
  }

  assertNotReached();
}

/**
 * Returns a getter and setter to be used as property descriptor in
 * Object.defineProperty(). When the setter changes the value a property change
 * event with the type {@code name + 'Change'} is fired.
 * @param {string} name The name of the property.
 * @param {PropertyKind=} kind What kind of underlying storage to use.
 * @param {function(?, ?):void=} setHook A function to run after the
 *     property is set, but before the propertyChange event is fired.
 */
export function getPropertyDescriptor(name, kind = PropertyKind.JS, setHook) {
  return {
    get: getGetter(name, kind),
    set: getSetter(name, kind, setHook),
  };
}
