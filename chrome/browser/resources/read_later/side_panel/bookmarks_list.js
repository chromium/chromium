// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_folder.js';

import {ListPropertyUpdateBehavior, ListPropertyUpdateBehaviorInterface} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {ListPropertyUpdateBehaviorInterface}
 */
const BookmarksListElementBase =
    mixinBehaviors([ListPropertyUpdateBehavior], PolymerElement);

/** @polymer */
export class BookmarksListElement extends BookmarksListElementBase {
  static get is() {
    return 'bookmarks-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
      folders_: {
        type: Array,
        value: [],
      },
    };
  }

  constructor() {
    super();

    /** @private @const {!BookmarksApiProxy} */
    this.bookmarksApi_ = BookmarksApiProxyImpl.getInstance();

    /** @private @const {!Object<string, !Function>} */
    this.listeners_ = {};
  }

  connectedCallback() {
    super.connectedCallback();
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;

      this.addListener_(
          'onChildrenReordered',
          (id, reorderedInfo) => this.onChildrenReordered_(id, reorderedInfo));
      this.addListener_(
          'onChanged', (id, changedInfo) => this.onChanged_(id, changedInfo));
      this.addListener_('onCreated', (id, node) => this.onCreated_(id, node));
      this.addListener_(
          'onMoved', (id, movedInfo) => this.onMoved_(id, movedInfo));
      this.addListener_(
          'onRemoved', (id, removeInfo) => this.onRemoved_(id, removeInfo));
    });
  }

  disconnectedCallback() {
    Object.keys(this.listeners_).forEach(eventName => {
      this.bookmarksApi_.callbackRouter[eventName].removeListener(
          this.listeners_[eventName]);
    });
  }

  /**
   * @param {string} eventName
   * @param {!Function} callback
   */
  addListener_(eventName, callback) {
    this.bookmarksApi_.callbackRouter[eventName].addListener(callback);
    this.listeners_[eventName] = callback;
  }

  /**
   * Finds the node within the nested array of folders and returns the path to
   * the node in the tree.
   * @param {string} id
   * @return {!Array<!chrome.bookmarks.BookmarkTreeNode>}
   * @private
   */
  findPathToId_(id) {
    const path = [];

    function findPathByIdInternal_(id, node) {
      if (node.id === id) {
        path.push(node);
        return true;
      }

      if (!node.children) {
        return false;
      }

      path.push(node);
      const foundInChildren =
          node.children.some(child => findPathByIdInternal_(id, child));
      if (!foundInChildren) {
        path.pop();
      }

      return foundInChildren;
    }

    this.folders_.some(folder => findPathByIdInternal_(id, folder));
    return path;
  }

  /**
   * Reduces an array of nodes to a string to notify Polymer of changes to the
   * nested array.
   * @param {!Array<!chrome.bookmarks.BookmarkTreeNode>} path
   * @return {string}
   * @private
   */
  getPathString_(path) {
    return path.reduce((reducedString, pathItem, index) => {
      if (index === 0) {
        return `folders_.${this.folders_.indexOf(pathItem)}`;
      }

      const parent = path[index - 1];
      return `${reducedString}.children.${parent.children.indexOf(pathItem)}`;
    }, '');
  }

  /**
   * @param {string} id
   * @param {{title: string, url: (string|undefined)}} changedInfo
   * @private
   */
  onChanged_(id, changedInfo) {
    console.log('changed!');
    const path = this.findPathToId_(id);
    Object.assign(path[path.length - 1], changedInfo);

    const pathString = this.getPathString_(path);
    Object.keys(changedInfo)
        .forEach(key => this.notifyPath(`${pathString}.${key}`));
  }

  /**
   *
   * @param {string} id
   * @param {{childIds: !Array<string>}} reorderedInfo
   * @private
   */
  onChildrenReordered_(id, reorderedInfo) {
    const path = this.findPathToId_(id);
    const parent = path[path.length - 1];
    const childById = parent.children.reduce((map, node) => {
      map[node.id] = node;
      return map;
    }, {});
    parent.children = reorderedInfo.childIds.map(id => childById[id]);
    const pathString = this.getPathString_(path);
    this.notifyPath(`${pathString}.children`);
  }

  /**
   * @param {string} id
   * @param {!chrome.bookmarks.BookmarkTreeNode} node
   * @private
   */
  onCreated_(id, node) {
    const pathToParent =
        this.findPathToId_(/** @type {string} */ (node.parentId));
    const pathToParentString = this.getPathString_(pathToParent);
    this.push(`${pathToParentString}.children`, node);
  }

  /**
   * @param {string} id
   * @param {{
   *    parentId: string,
   *    index: number,
   *    oldParentId: string,
   *    oldIndex: number
   * }} movedInfo
   * @private
   */
  onMoved_(id, movedInfo) {
    // Get old path and remove node from oldParent at oldIndex.
    const oldParentPath = this.findPathToId_(movedInfo.oldParentId);
    const oldParentPathString = this.getPathString_(oldParentPath);
    const oldParent = oldParentPath[oldParentPath.length - 1];
    const movedNode = oldParent.children[movedInfo.oldIndex];
    Object.assign(
        movedNode, {index: movedInfo.index, parentId: movedInfo.parentId});
    this.splice(`${oldParentPathString}.children`, movedInfo.oldIndex, 1);

    // Get new parent's path and add the node to the new parent at index.
    const newParentPath = this.findPathToId_(movedInfo.parentId);
    const newParentPathString = this.getPathString_(newParentPath);
    this.splice(
        `${newParentPathString}.children`, movedInfo.index, 0, movedNode);
  }

  /**
   * @param {string} id
   * @param {{parentId: string, index: number}} removeInfo
   * @private
   */
  onRemoved_(id, removeInfo) {
    const oldPath = this.findPathToId_(id);
    const removedNode = oldPath.pop();
    const oldParent = oldPath[oldPath.length - 1];
    const oldParentPathString = this.getPathString_(oldPath);
    this.splice(
        `${oldParentPathString}.children`,
        oldParent.children.indexOf(removedNode), 1);
  }
}

customElements.define(BookmarksListElement.is, BookmarksListElement);