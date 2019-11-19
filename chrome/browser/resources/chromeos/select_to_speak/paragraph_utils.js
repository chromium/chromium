// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationNode = chrome.automation.AutomationNode;
var RoleType = chrome.automation.RoleType;

/**
 * @constructor
 */
let ParagraphUtils = function() {};

/**
 * Gets the first ancestor of a node which is a paragraph or is not inline,
 * or get the root node if none is found.
 * @param {AutomationNode} node The node to get the parent for.
 * @return {?AutomationNode} the parent paragraph or null if there is none.
 */
ParagraphUtils.getFirstBlockAncestor = function(node) {
  let parent = node.parent;
  let root = node.root;
  while (parent != null) {
    if (parent == root) {
      return parent;
    }
    if (parent.role == RoleType.PARAGRAPH || parent.role == RoleType.SVG_ROOT) {
      return parent;
    }
    if (parent.display !== undefined && parent.display != 'inline' &&
        parent.role != RoleType.STATIC_TEXT &&
        (parent.parent && parent.parent.role != RoleType.SVG_ROOT)) {
      return parent;
    }
    parent = parent.parent;
  }
  return null;
};

/**
 * Determines whether two nodes are in the same block-like ancestor, i.e.
 * whether they are in the same paragraph.
 * @param {AutomationNode|undefined} first The first node to compare.
 * @param {AutomationNode|undefined} second The second node to compare.
 * @return {boolean} whether two nodes are in the same paragraph.
 */
ParagraphUtils.inSameParagraph = function(first, second) {
  if (first === undefined || second === undefined) {
    return false;
  }
  // TODO: Clean up this check after crbug.com/774308 is resolved.
  // At that point we will only need to check for display:block or inline-block.
  if (((first.display == 'block' || first.display == 'inline-block') &&
       first.role != RoleType.STATIC_TEXT &&
       first.role != RoleType.INLINE_TEXT_BOX) ||
      ((second.display == 'block' || second.display == 'inline-block') &&
       second.role != RoleType.STATIC_TEXT &&
       second.role != RoleType.INLINE_TEXT_BOX)) {
    // 'block' or 'inline-block' elements cannot be in the same paragraph.
    return false;
  }
  let firstBlock = ParagraphUtils.getFirstBlockAncestor(first);
  let secondBlock = ParagraphUtils.getFirstBlockAncestor(second);
  return firstBlock != undefined && firstBlock == secondBlock;
};

/**
 * Determines whether a string is only whitespace.
 * @param {string|undefined} name A string to test
 * @return {boolean} whether the string is only whitespace
 */
ParagraphUtils.isWhitespace = function(name) {
  if (name === undefined || name.length == 0) {
    return true;
  }
  // Search for one or more whitespace characters
  let re = /^\s+$/;
  return re.exec(name) != null;
};

/**
 * Gets the text to be read aloud for a particular node.
 * @param {!AutomationNode} node
 * @return {string} The text to read for this node.
 */
ParagraphUtils.getNodeName = function(node) {
  if (node.role === RoleType.TEXT_FIELD &&
      (node.children === undefined || node.children.length === 0) &&
      node.value) {
    // A text field with no children should use its value instead of
    // the name element, this is the contents of the text field.
    // This occurs in native UI such as the omnibox.
    return node.value;
  } else if (
      node.role === RoleType.CHECK_BOX ||
      node.role === RoleType.MENU_ITEM_CHECK_BOX) {
    let stateString = chrome.i18n.getMessage(
        'select_to_speak_checkbox_' +
        (node.checked === 'true' ?
             'checked' :
             (node.checked === 'mixed' ? 'mixed' : 'unchecked')));
    return !ParagraphUtils.isWhitespace(node.name) ?
        node.name + ' ' + stateString :
        stateString;
  } else if (
      node.role === RoleType.RADIO_BUTTON ||
      node.role === RoleType.MENU_ITEM_RADIO) {
    let stateString = chrome.i18n.getMessage(
        'select_to_speak_radiobutton_' +
        (node.checked === 'true' ?
             'selected' :
             (node.checked === 'mixed' ? 'mixed' : 'unselected')));
    return !ParagraphUtils.isWhitespace(node.name) ?
        node.name + ' ' + stateString :
        stateString;
  }
  return node.name ? node.name : '';
};

/**
 * Determines the index into the parent name at which the inlineTextBox
 * node name begins.
 * @param {AutomationNode} inlineTextNode An inlineTextBox type node.
 * @return {number} The character index into the parent node at which
 *     this node begins.
 */
ParagraphUtils.getStartCharIndexInParent = function(inlineTextNode) {
  let result = 0;
  for (let i = 0; i < inlineTextNode.indexInParent; i++) {
    result += inlineTextNode.parent.children[i].name.length;
  }
  return result;
};

/**
 * Determines the inlineTextBox child of a staticText node that appears
 * at the given character index into the name of the staticText node. Uses
 * the inlineTextBoxes name length to determine position. For example, if
 * a staticText has name "abc 123" and two children with names "abc " and
 * "123", indexes 0-3 would return the first child and indexes 4+ would
 * return the second child.
 * @param {AutomationNode} staticTextNode The staticText node to search.
 * @param {number} index The index into the staticTextNode's name.
 * @return {?AutomationNode} The inlineTextBox node within the staticText
 *    node that appears at this index into the staticText node's name, or
 *    the last inlineTextBox in the staticText node if the index is too
 *    large.
 */
ParagraphUtils.findInlineTextNodeByCharacterIndex = function(
    staticTextNode, index) {
  if (staticTextNode.children.length == 0) {
    return null;
  }
  let textLength = 0;
  for (var i = 0; i < staticTextNode.children.length; i++) {
    let node = staticTextNode.children[i];
    if (node.name.length + textLength > index) {
      return node;
    }
    textLength += node.name.length;
  }
  return staticTextNode.children[staticTextNode.children.length - 1];
};

/**
 * Builds information about nodes in a group until it reaches the end of the
 * group. It may return a NodeGroup with a single node, or a large group
 * representing a paragraph of inline nodes.
 * @param {Array<!AutomationNode>} nodes List of automation nodes to use.
 * @param {number} index The index into nodes at which to start.
 * @param {boolean} splitOnLanguage flag to determine if we should split nodes
 *                  up based on language.
 * @return {ParagraphUtils.NodeGroup} info about the node group
 */
ParagraphUtils.buildNodeGroup = function(nodes, index, splitOnLanguage) {
  let node = nodes[index];
  let next = nodes[index + 1];
  let result = new ParagraphUtils.NodeGroup(
      ParagraphUtils.getFirstBlockAncestor(nodes[index]));
  let staticTextParent = null;
  let currentLanguage = undefined;
  // TODO: Don't skip nodes. Instead, go through every node in
  // this paragraph from the first to the last in the nodes list.
  // This will catch nodes at the edges of the user's selection like
  // short links at the beginning or ends of sentences.
  //
  // While next node is in the same paragraph as this node AND is
  // a text type node, continue building the paragraph.
  while (index < nodes.length) {
    let name = ParagraphUtils.getNodeName(node);
    if (!ParagraphUtils.isWhitespace(name)) {
      let newNode;
      if (node.role == RoleType.INLINE_TEXT_BOX && node.parent !== undefined) {
        if (node.parent.role == RoleType.STATIC_TEXT) {
          // This is an inlineTextBox node with a staticText parent. If that
          // parent is already added to the result, we can skip. This adds
          // each parent only exactly once.
          if (staticTextParent && staticTextParent.node !== node.parent) {
            // We are on a new staticText. Make a new parent to add to.
            staticTextParent = null;
          }
          if (staticTextParent === null) {
            staticTextParent = new ParagraphUtils.NodeGroupItem(
                node.parent, result.text.length, true);
            newNode = staticTextParent;
          }
        } else {
          // Not an staticText parent node. Add it directly.
          newNode =
              new ParagraphUtils.NodeGroupItem(node, result.text.length, false);
        }
      } else {
        // Not an inlineTextBox node. Add it directly, as each node in this list
        // is relevant.
        newNode =
            new ParagraphUtils.NodeGroupItem(node, result.text.length, false);
      }
      if (newNode) {
        result.text += ParagraphUtils.getNodeName(newNode.node) + ' ';
        result.nodes.push(newNode);
      }
    }

    // Set currentLanguage if we don't have one yet.
    // We have to do this before we consider stopping otherwise we miss out on
    // the last language attribute of each NodeGroup which could be important if
    // this NodeGroup only contains a single node, or if all previous nodes
    // lacked any language information.
    if (!currentLanguage) {
      currentLanguage = node.detectedLanguage;
    }

    // Stop if any of following is true:
    //  1. we have no more nodes to process.
    //  2. the next node is not part of the same paragraph.
    if (index + 1 >= nodes.length ||
        !ParagraphUtils.inSameParagraph(node, next)) {
      break;
    }

    // Stop if the next node would change our currentLanguage (if we have
    // one). We allow an undefined detectedLanguage to match with any previous
    // language, so that we will never break up a NodeGroup on an undefined
    // detectedLanguage.
    if (splitOnLanguage && currentLanguage && next.detectedLanguage &&
        currentLanguage !== next.detectedLanguage) {
      break;
    }

    index += 1;
    node = next;
    next = nodes[index + 1];
  }

  if (splitOnLanguage && currentLanguage) {
    result.detectedLanguage = currentLanguage;
  }
  result.endIndex = index;
  return result;
};

/**
 * Class representing a node group, which may be a single node or a
 * full paragraph of nodes.
 *
 * @param {?AutomationNode} blockParent The first block ancestor of
 *     this group. This may be the paragraph parent, for example.
 * @constructor
 */
ParagraphUtils.NodeGroup = function(blockParent) {
  /**
   * Full text of this paragraph.
   * @type {string}
   */
  this.text = '';

  /**
   * List of nodes in this paragraph in order.
   * @type {Array<ParagraphUtils.NodeGroupItem>}
   */
  this.nodes = [];

  /**
   * The block parent of this NodeGroup, if there is one.
   * @type {?AutomationNode}
   */
  this.blockParent = blockParent;

  /**
   * The index of the last node in this paragraph from the list of
   * nodes originally selected by the user.
   * Note that this may not be stable over time, because nodes may
   * come and go from the automation tree. This should not be used
   * in any callbacks / asynchronously.
   * @type {number}
   */
  this.endIndex = -1;

  /**
   * Language and country code for all nodes within this NodeGroup.
   * @type {string|undefined}
   */
  this.detectedLanguage = undefined;
};

/**
 * Class representing an automation node within a block of text, like
 * a paragraph. Each Item in a NodeGroup has a start index within the
 * total text, as well as the original AutomationNode it was associated
 * with.
 *
 * @param {!AutomationNode} node The AutomationNode associated with this item
 * @param {number} startChar The index into the NodeGroup's text string where
 *     this item begins.
 * @param {boolean=} opt_hasInlineText If this NodeGroupItem has inlineText
 *     children.
 * @constructor
 */
ParagraphUtils.NodeGroupItem = function(node, startChar, opt_hasInlineText) {
  /**
   * @type {!AutomationNode}
   */
  this.node = node;

  /**
   * The index into the NodeGroup's text string that is the first character
   * of the text of this automation node.
   * @type {number}
   */
  this.startChar = startChar;

  /**
   * If this is a staticText node which has inlineTextBox children which should
   * be selected. We cannot select the inlineTextBox children directly because
   * they are not guarenteed to be stable.
   * @type {boolean}
   */
  this.hasInlineText =
      opt_hasInlineText !== undefined ? opt_hasInlineText : false;
};
