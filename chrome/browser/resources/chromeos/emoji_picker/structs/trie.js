// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Trie {
  constructor() {
    /** @private {!Object<string, !Trie>} */
    this.children_ = {};
    /** @private {boolean} */
    this.isEndOfWord_ = false;
  }

  /**
   * Adds a term to the trie.
   * @param {string} key the term
   */
  add(key) {
    let node = this;
    for (const curChar of key) {
      if (node.children_[curChar] === undefined) {
        node.children_[curChar] = new Trie();
      }
      node = node.children_[curChar];
    }
    node.isEndOfWord_ = true;
  }

  /**
   * @private
   * @param {string} path the term
   * @returns {Trie | undefined } returns the node at the end of the term, or
   *     returns undefined if the node for the path does not exist.
   */
  getChildNode_(path) {
    let node = this;
    for (const curChar of path) {
      node = node.children_[curChar];
      if (node === undefined) {
        return undefined;
      }
    }
    return node;
  }

  /**
   * @param {string | undefined} prefix
   * @returns {!Array<string>} all keys that share the same given prefix.
   */
  getKeys(prefix) {
    const allKeys = [];
    if (prefix !== undefined) {
      const prefixNode = this.getChildNode_(prefix);
      if (prefixNode === undefined) {
        return [];
      }
      prefixNode.getKeysInternal_(prefix, allKeys);
    } else {
      this.getKeysInternal_('', allKeys);
    }
    return allKeys;
  }

  /**
   * Collects all keys in the trie that has 'curKey' as the prefix.
   * @private
   * @param {string} curKey
   * @param {!Array<string>} allKeys
   */
  getKeysInternal_(curKey, allKeys) {
    if (this.isEndOfWord_) {
      allKeys.push(curKey);
    }
    for (const char in this.children_) {
      this.children_[char].getKeysInternal_(`${curKey}${char}`, allKeys);
    }
  }

  /**
   * Returns true if the trie contains the given key, otherwise false.
   * @param {string} key
   * @returns {boolean}
   */
  containsKey(key) {
    const node = this.getChildNode_(key);
    return node !== undefined && node.isEndOfWord_ === true;
  }

  /**
   * @private
   * @returns {boolean} Returns true if the trie has children nodes, otherwise
   *     returns false.
   */
  hasChildren_() {
    return Object.keys(this.children_).length > 0;
  }

  /**
   * Erase all content of the trie.
   */
  clear() {
    this.children_ = {};
    this.isEndOfWord_ = false;
  }
}