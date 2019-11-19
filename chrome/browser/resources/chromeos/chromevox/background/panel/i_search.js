// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects related to incremental search.
 */

goog.provide('ISearch');
goog.provide('ISearchHandler');
goog.provide('ISearchUI');

goog.require('AutomationUtil');
goog.require('ChromeVoxState');
goog.require('Output');
goog.require('constants');
goog.require('cursors.Cursor');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var RoleType = chrome.automation.RoleType;

/**
 * An interface implemented by objects that wish to handle events related to
 * incremental search.
 * @interface
 */
ISearchHandler = function() {};

ISearchHandler.prototype = {
  /**
   * Called when there are no remaining nodes in the document matching
   * search.
   * @param {!AutomationNode} boundaryNode The last node before reaching either
   * the start or end of the document.
   */
  onSearchReachedBoundary: function(boundaryNode) {},

  /**
   * Called when search result node changes.
   * @param {!AutomationNode} node The new search result.
   * @param {number} start The index into the name where the search match
   *     starts.
   * @param {number} end The index into the name where the search match ends.
   */
  onSearchResultChanged: function(node, start, end) {}
};

/**
 * Controls an incremental search.
 * @param {!cursors.Cursor} cursor
 * @constructor
 */
ISearch = function(cursor) {
  if (!cursor.node) {
    throw 'Incremental search started from invalid range.';
  }

  var leaf = AutomationUtil.findNodePre(
                 cursor.node, Dir.FORWARD, AutomationPredicate.leaf) ||
      cursor.node;

  /** @type {!cursors.Cursor} */
  this.cursor = cursors.Cursor.fromNode(leaf);

  /** @private {number} */
  this.callbackId_ = 0;

  // Global exports.
  /** Exported for this background script. */
  ChromeVox = chrome.extension.getBackgroundPage()['ChromeVox'];
};

ISearch.prototype = {
  /**
   * @param {!ISearchHandler} handler
   */
  set handler(handler) {
    this.handler_ = handler;
  },

  /**
   * Performs a search.
   * @param {string} searchStr
   * @param {Dir} dir
   * @param {boolean=} opt_nextObject
   */
  search: function(searchStr, dir, opt_nextObject) {
    clearTimeout(this.callbackId_);
    var step = function() {
      searchStr = searchStr.toLocaleLowerCase();
      var node = this.cursor.node;
      var result = node;

      if (opt_nextObject) {
        // We want to start/continue the search at the next object.
        result =
            AutomationUtil.findNextNode(node, dir, AutomationPredicate.object);
      }

      do {
        // Ask native to search the underlying data for a performance boost.
        result = result.getNextTextMatch(searchStr, dir == Dir.BACKWARD);
      } while (result && !AutomationPredicate.object(result));

      if (result) {
        this.cursor = cursors.Cursor.fromNode(result);
        var start = result.name.toLocaleLowerCase().indexOf(searchStr);
        var end = start + searchStr.length;
        this.handler_.onSearchResultChanged(result, start, end);
      } else {
        this.handler_.onSearchReachedBoundary(this.cursor.node);
      }
    };

    this.callbackId_ = setTimeout(step.bind(this), 0);
  },

  clear: function() {
    clearTimeout(this.callbackId_);
  }
};

/**
 * @param {Element} input
 * @constructor
 * @implements {ISearchHandler}
 */
ISearchUI = function(input) {
  /** @type {ChromeVoxState} @private */
  this.background_ =
      chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
  this.iSearch_ = new ISearch(this.background_.currentRange.start);
  this.input_ = input;
  this.dir_ = Dir.FORWARD;
  this.iSearch_.handler = this;

  this.onKeyDown = this.onKeyDown.bind(this);
  this.onTextInput = this.onTextInput.bind(this);

  input.addEventListener('keydown', this.onKeyDown, true);
  input.addEventListener('textInput', this.onTextInput, false);
};

/**
 * @param {Element} input
 * @return {ISearchUI}
 */
ISearchUI.init = function(input) {
  if (ISearchUI.instance_) {
    ISearchUI.instance_.destroy();
  }

  if (!input) {
    throw 'Expected search input';
  }

  ISearchUI.instance_ = new ISearchUI(input);
  input.focus();
  input.select();
  return ISearchUI.instance_;
};

ISearchUI.prototype = {
  /**
   * Listens to key down events.
   * @param {Event} evt
   * @return {boolean}
   */
  onKeyDown: function(evt) {
    switch (evt.key) {
      case 'ArrowUp':
        this.dir_ = Dir.BACKWARD;
        break;
      case 'ArrowDown':
        this.dir_ = Dir.FORWARD;
        break;
      case 'Escape':
        Panel.closeMenusAndRestoreFocus();
        return false;
      case 'Enter':
        Panel.setPendingCallback(function() {
          var node = this.iSearch_.cursor.node;
          if (!node) {
            return;
          }
          chrome.extension.getBackgroundPage()
              .ChromeVoxState.instance['navigateToRange'](
                  cursors.Range.fromNode(node));
        }.bind(this));
        Panel.closeMenusAndRestoreFocus();
        return false;
      default:
        return false;
    }
    this.iSearch_.search(this.input_.value, this.dir_, true);
    evt.preventDefault();
    evt.stopPropagation();
    return false;
  },

  /**
   * Listens to text input events.
   * @param {Event} evt
   * @return {boolean}
   */
  onTextInput: function(evt) {
    var searchStr = evt.target.value + evt.data;
    this.iSearch_.clear();
    this.iSearch_.search(searchStr, this.dir_);
    return true;
  },

  /**
   * @override
   */
  onSearchReachedBoundary: function(boundaryNode) {
    this.output_(boundaryNode);
    ChromeVox.earcons.playEarcon(Earcon.WRAP);
  },

  /**
   * @override
   */
  onSearchResultChanged: function(node, start, end) {
    this.output_(node, start, end);
  },

  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_start
   * @param {number=} opt_end
   * @private
   */
  output_: function(node, opt_start, opt_end) {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    var o = new Output();
    if (opt_start && opt_end) {
      o.withString([
        node.name.substr(0, opt_start),
        node.name.substr(opt_start, opt_end - opt_start),
        node.name.substr(opt_end)
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          cursors.Range.fromNode(node), null, Output.EventType.NAVIGATE);
    }
    o.go();

    this.background_.setCurrentRange(cursors.Range.fromNode(node));
  },

  /** Unregisters event handlers. */
  destroy: function() {
    this.iSearch_.handler_ = null;
    this.iSearch_ = null;
    var input = this.input_;
    this.input_ = null;
    input.removeEventListener('keydown', this.onKeyDown, true);
    input.removeEventListener('textInput', this.onTextInput, false);
  }
};
});  // goog.scope
