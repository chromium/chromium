// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SwitchAccess} from './switch_access.js';

/**
 * Stores all identifying attributes for an AutomationNode.
 * A helper object for NodeIdentifier.
 * @typedef {{
 *    id: string,
 *    name: string,
 *    role: string,
 *    childCount: number,
 *    indexInParent: number,
 *    className: string,
 *    htmlTag: string }}
 */
let Attributes;

/** A class used to identify AutomationNodes. */
export class NodeIdentifier {
  /**
   * @param {!{
   *           attributes: !Attributes,
   *           pageUrl: string}} params
   * @private
   */
  constructor(params) {
    if (!SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      throw new Error(
          'Multistep automation flag must be enabled to access ActionRecorder');
    }

    /** @type {!Attributes} */
    this.attributes = params.attributes;
    /** @type {string} */
    this.pageUrl = params.pageUrl;
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @return {!NodeIdentifier}
   */
  static fromNode(node) {
    const params = {
      attributes: NodeIdentifier.createAttributes_(node),
      pageUrl: node.root.docUrl || '',
    };

    return new NodeIdentifier(params);
  }

  /**
   * Returns true if |this| is equal to |other|.
   * @param {!NodeIdentifier} other
   * @return {boolean}
   */
  equals(other) {
    // If pageUrl and HTML Id match, we know they refer to the same node.
    if (this.pageUrl && this.attributes.id && this.pageUrl === other.pageUrl &&
        this.attributes.id === other.attributes.id) {
      return true;
    }

    // TODO: Implement better matching algorithm.
    // Ensure both NodeIdentifiers are composed of matching Attributes.
    if (!this.matchingAttributes_(this.attributes, other.attributes)) {
      return false;
    }

    return true;
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @return {!Attributes}
   * @private
   */
  static createAttributes_(node) {
    return {
      id: (node.htmlAttributes) ? node.htmlAttributes['id'] || '' : '',
      name: node.name || '',
      role: node.role || '',
      childCount: node.childCount || 0,
      indexInParent: node.indexInParent || 0,
      className: node.className || '',
      htmlTag: node.htmlTag || ''
    };
  }

  /**
   * @param {!Attributes} target
   * @param {!Attributes} candidate
   * @return {boolean}
   * @private
   */
  matchingAttributes_(target, candidate) {
    for (const [key, targetValue] of Object.entries(target)) {
      if (candidate[key] !== targetValue) {
        return false;
      }
    }
    return true;
  }

  /** @override */
  toString() {
    return JSON.stringify(this);
  }
}
