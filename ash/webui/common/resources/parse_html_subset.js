// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from './assert.js';

/**
 * @typedef {{
 *   substitutions: (!Array<string>|undefined),
 *   attrs: (!Array<string>|undefined),
 *   tags: (!Array<string>|undefined),
 * }}
 */
export let SanitizeInnerHtmlOpts;

/**
 * Make a string safe for Polymer bindings that are inner-h-t-m-l or other
 * innerHTML use.
 * @param {string} rawString The unsanitized string
 * @param {SanitizeInnerHtmlOpts=} opts Optional additional allowed tags and
 *     attributes.
 * @return {string}
 */
export const sanitizeInnerHtmlInternal = function(rawString, opts) {
  opts = opts || {};
  return parseHtmlSubset(`<b>${rawString}</b>`, opts.tags, opts.attrs)
      .firstChild.innerHTML;
};

let sanitizedPolicy = null;

/**
 * Same as |sanitizeInnerHtmlInternal|, but it passes through sanitizedPolicy
 * to create a TrustedHTML.
 * TrustedTypePolicy: createHTML() takes an optional array but our usage for
 * sanitizeInnerHtml uses a singular opt argument. We specify the first element.
 * @param {string} rawString The unsanitized string
 * @param {SanitizeInnerHtmlOpts=} opts Optional additional allowed tags and
 *     attributes.
 * @return {TrustedHTML}
 */
export function sanitizeInnerHtml(rawString, opts) {
  assert(window.trustedTypes);
  if (sanitizedPolicy === null) {
    // Initialize |sanitizedPolicy| lazily.
    sanitizedPolicy =
        window.trustedTypes.createPolicy('ash-deprecated-sanitize-inner-html', {
          createHTML: (string, ...opts) =>
              sanitizeInnerHtmlInternal(string, opts[0]),
          createScript: (message) => assertNotReached(message),
          createScriptURL: (message) => assertNotReached(message),
        });
  }
  return sanitizedPolicy.createHTML(rawString, opts);
}

/**
 * Parses a very small subset of HTML. This ensures that insecure HTML /
 * javascript cannot be injected into WebUI.
 * @param {string} s The string to parse.
 * @param {!Array<string>=} extraTags Optional extra allowed tags.
 * @param {!Array<string>=} extraAttrs
 *     Optional extra allowed attributes (all tags are run through these).
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
export const parseHtmlSubset = (function() {
  'use strict';

  /** @typedef {function(!Node, string):boolean} */
  let AllowFunction;

  /** @type {!AllowFunction} */
  const allowAttribute = (node, value) => true;

  /**
   * Allow-list of attributes in parseHtmlSubset.
   * @type {!Map<string, !AllowFunction>}
   * @const
   */
  const allowedAttributes = new Map([
    [
      'href',
      (node, value) => {
        // Only allow a[href] starting with chrome:// or https:// or equaling
        // to #.
        return node.tagName === 'A' &&
            (value.startsWith('chrome://') || value.startsWith('https://') ||
             value === '#');
      },
    ],
    [
      'target',
      (node, value) => {
        // Only allow a[target='_blank'].
        // TODO(dbeam): are there valid use cases for target !== '_blank'?
        return node.tagName === 'A' && value === '_blank';
      },
    ],
  ]);

  /**
   * Allow-list of optional attributes in parseHtmlSubset.
   * @type {!Map<string, !AllowFunction>}
   * @const
   */
  const allowedOptionalAttributes = new Map([
    ['class', allowAttribute],
    ['id', allowAttribute],
    ['is', (node, value) => value === 'action-link' || value === ''],
    ['role', (node, value) => value === 'link'],
    [
      'src',
      (node, value) => {
        // Only allow img[src] starting with chrome://
        return node.tagName === 'IMG' && value.startsWith('chrome://');
      },
    ],
    ['tabindex', allowAttribute],
    ['aria-hidden', allowAttribute],
    ['aria-labelledby', allowAttribute],
  ]);

  /**
   * Allow-list of tag names in parseHtmlSubset.
   * @type {!Set<string>}
   * @const
   */
  const allowedTags = new Set(
      ['A', 'B', 'I', 'BR', 'DIV', 'EM', 'KBD', 'P', 'PRE', 'SPAN', 'STRONG']);

  /**
   * Allow-list of optional tag names in parseHtmlSubset.
   * @type {!Set<string>}
   * @const
   */
  const allowedOptionalTags = new Set(['IMG', 'LI', 'UL']);

  /**
   * This policy maps a given string to a `TrustedHTML` object
   * without performing any validation. Callsites must ensure
   * that the resulting object will only be used in inert
   * documents. Initialized lazily.
   * @type {!TrustedTypePolicy}
   */
  let unsanitizedPolicy;

  /**
   * @param {!Array<string>} optTags an Array to merge.
   * @return {!Set<string>} Set of allowed tags.
   */
  function mergeTags(optTags) {
    const clone = new Set(allowedTags);
    optTags.forEach(str => {
      const tag = str.toUpperCase();
      if (allowedOptionalTags.has(tag)) {
        clone.add(tag);
      }
    });
    return clone;
  }

  /**
   * @param {!Array<string>} optAttrs an Array to merge.
   * @return {!Map<string, !AllowFunction>} Map of allowed
   *     attributes.
   */
  function mergeAttrs(optAttrs) {
    const clone = new Map([...allowedAttributes]);
    optAttrs.forEach(key => {
      if (allowedOptionalAttributes.has(key)) {
        clone.set(key, allowedOptionalAttributes.get(key));
      }
    });
    return clone;
  }

  function walk(n, f) {
    f(n);
    for (let i = 0; i < n.childNodes.length; i++) {
      walk(n.childNodes[i], f);
    }
  }

  function assertElement(tags, node) {
    if (!tags.has(node.tagName)) {
      throw Error(node.tagName + ' is not supported');
    }
  }

  function assertAttribute(attrs, attrNode, node) {
    const n = attrNode.nodeName;
    const v = attrNode.nodeValue;
    if (!attrs.has(n) || !attrs.get(n)(node, v)) {
      throw Error(node.tagName + '[' + n + '="' + v + '"] is not supported');
    }
  }

  return function(s, extraTags, extraAttrs) {
    const tags = extraTags ? mergeTags(extraTags) : allowedTags;
    const attrs = extraAttrs ? mergeAttrs(extraAttrs) : allowedAttributes;

    const doc = document.implementation.createHTMLDocument('');
    const r = doc.createRange();
    r.selectNode(doc.body);

    if (window.trustedTypes) {
      if (!unsanitizedPolicy) {
        unsanitizedPolicy = trustedTypes.createPolicy(
            'ash-deprecated-parse-html-subset',
            {createHTML: untrustedHTML => untrustedHTML});
      }
      s = unsanitizedPolicy.createHTML(s);
    }

    // This does not execute any scripts because the document has no view.
    const df = r.createContextualFragment(s);
    walk(df, function(node) {
      switch (node.nodeType) {
        case Node.ELEMENT_NODE:
          assertElement(tags, node);
          const nodeAttrs = node.attributes;
          for (let i = 0; i < nodeAttrs.length; ++i) {
            assertAttribute(attrs, nodeAttrs[i], node);
          }
          break;

        case Node.COMMENT_NODE:
        case Node.DOCUMENT_FRAGMENT_NODE:
        case Node.TEXT_NODE:
          break;

        default:
          throw Error('Node type ' + node.nodeType + ' is not supported');
      }
    });
    return df;
  };
})();
