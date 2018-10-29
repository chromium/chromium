// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets the width (in pixels) on a DOM node.
 * @param {!HtmlNode} node The node whose width is being set.
 * @param {number} widthPx The width in pixels.
 */
function setNodeWidth(node, widthPx) {
  node.style.width = widthPx.toFixed(0) + 'px';
}

/**
 * Sets the height (in pixels) on a DOM node.
 * @param {!HtmlNode} node The node whose height is being set.
 * @param {number} heightPx The height in pixels.
 */
function setNodeHeight(node, heightPx) {
  node.style.height = heightPx.toFixed(0) + 'px';
}

/**
 * Sets the position and size of a DOM node (in pixels).
 * @param {!HtmlNode} node The node being positioned.
 * @param {number} leftPx The left position in pixels.
 * @param {number} topPx The top position in pixels.
 * @param {number} widthPx The width in pixels.
 * @param {number} heightPx The height in pixels.
 */
function setNodePosition(node, leftPx, topPx, widthPx, heightPx) {
  node.style.left = leftPx.toFixed(0) + 'px';
  node.style.top = topPx.toFixed(0) + 'px';
  setNodeWidth(node, widthPx);
  setNodeHeight(node, heightPx);
}

/**
 * Sets the visibility for a DOM node.
 * @param {!HtmlNode} node The node being positioned.
 * @param {boolean} isVisible Whether to show the node or not.
 */
function setNodeDisplay(node, isVisible) {
  node.style.display = isVisible ? '' : 'none';
}

/**
 * Toggles the visibility of a DOM node.
 * @param {!HtmlNode} node The node to show or hide.
 */
function toggleNodeDisplay(node) {
  setNodeDisplay(node, !getNodeDisplay(node));
}

/**
 * Returns the visibility of a DOM node.
 * @param {!HtmlNode} node The node to query.
 */
function getNodeDisplay(node) {
  return node.style.display != 'none';
}

/**
 * Adds a node to |parentNode|, of type |tagName|.
 * @param {!HtmlNode} parentNode The node that will be the parent of the new
 *     element.
 * @param {string} tagName the tag name of the new element.
 * @return {!HtmlElement} The newly created element.
 */
function addNode(parentNode, tagName) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  return elem;
}

/**
 * Adds |text| to node |parentNode|.
 * @param {!HtmlNode} parentNode The node to add text to.
 * @param {string} text The text to be added.
 * @return {!Object} The newly created text node.
 */
function addTextNode(parentNode, text) {
  var textNode = parentNode.ownerDocument.createTextNode(text);
  parentNode.appendChild(textNode);
  return textNode;
}

/**
 * Adds a node to |parentNode|, of type |tagName|.  Then adds
 * |text| to the new node.
 * @param {!HtmlNode} parentNode The node that will be the parent of the new
 *     element.
 * @param {string} tagName the tag name of the new element.
 * @param {string} text The text to be added.
 * @return {!HtmlElement} The newly created element.
 */
function addNodeWithText(parentNode, tagName, text) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  addTextNode(elem, text);
  return elem;
}

/**
 * Helper to make sure singleton classes are not instantiated more than once.
 * @param {Function} ctor The constructor function being checked.
 */
function assertFirstConstructorCall(ctor) {
  // This is the variable which is set by cr.addSingletonGetter().
  if (ctor.hasCreateFirstInstance_) {
    throw Error(
        'The class ' + ctor.name + ' is a singleton, and should ' +
        'only be accessed using ' + ctor.name + '.getInstance().');
  }
  ctor.hasCreateFirstInstance_ = true;
}

function hasTouchScreen() {
  return 'ontouchstart' in window;
}
