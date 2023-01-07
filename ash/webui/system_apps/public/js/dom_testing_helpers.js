// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint no-var: "off" */

/**
 * @fileoverview Helpers for testing against DOM. For integration tests, this is
 * injected into an isolated world, so can't access objects in other scripts.
 */

/**
 * Descends down from `document.body` using queries in `shadowRootPath` and
 * `selectorMethod`. The shadow root of each node found is used to descend
 * further.
 *
 * @param {!function(string, !Array<string>=):!Promise<!Element|undefined>}
 *     selectorMethod
 * @param {!Array<string>=} shadowRootPath
 * @returns {!Promise<!HTMLElement|!ShadowRoot>}
 */
var getNextRoot = async (selectorMethod, shadowRootPath = []) => {
  /** @type {!HTMLElement|!ShadowRoot} */
  let parentNode = document.body;
  const parentQuery = shadowRootPath.shift();
  if (parentQuery) {
    const element = await selectorMethod(parentQuery, shadowRootPath);
    if (!(element instanceof HTMLElement) || !element.shadowRoot) {
      throw new Error('Path not a shadow root HTMLElement');
    }
    parentNode = element.shadowRoot;
  }
  return parentNode;
};

/**
 * Runs a query selector once. Returns the Element if it's found, otherwise
 * returns undefined.
 *
 * @param {string} query
 * @param {!Array<string>=} path
 * @return {!Promise<!Element|undefined>}
 */
var getNode = async (query, path = []) => {
  const parentElement = await getNextRoot(getNode, path);
  const existingElement = parentElement.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  return Promise.resolve(undefined);
};

/**
 * Runs a query selector until it finds an element (repeated on each mutation).
 * If the element does not exist this will timeout.
 *
 * opt_path defines the path of ancestor Elements to the queried Element, whose
 * shadow boundaries need to be crossed to find the queried Element. These must
 * be defined in order from closest parent of the queried Element, to the
 * ancestor that is in the document.body subtree.
 * If opt_path is not defined correctly this will timeout.
 *
 * @param {string} query
 * @param {!Array<string>=} opt_path
 * @return {!Promise<!Element>}
 */
var waitForNode = async (query, opt_path) => {
  const parentElement = await getNextRoot(waitForNode, opt_path);
  const existingElement = parentElement.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  console.log('Waiting for ' + query);
  return new Promise(resolve => {
    const observer = new MutationObserver((mutationList, observer) => {
      const element = parentElement.querySelector(query);
      if (element) {
        resolve(element);
        observer.disconnect();
      }
    });
    observer.observe(
        parentElement, {attributes: true, childList: true, subtree: true});
  });
};

/**
 * Returns a promise that resolves when the passed node's child list is updated
 * (a child is added or removed).
 * @param {!Node} node
 * @return {!Promise}
 */
var childListUpdate = (node) => {
  return new Promise(resolve => {
    const observer = new MutationObserver(() => {
      resolve();
      observer.disconnect();
    });
    observer.observe(node, {childList: true});
  });
};
