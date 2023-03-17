// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Predicates for the automation extension API.
 */

import {constants} from './constants.js';

const AutomationNode = chrome.automation.AutomationNode;
const InvalidState = chrome.automation.InvalidState;
const MarkerType = chrome.automation.MarkerType;
const Restriction = chrome.automation.Restriction;
const Role = chrome.automation.RoleType;
const State = chrome.automation.StateType;

/**
 * A helper to check if |node| or any descendant is actionable.
 * @param {!AutomationNode} node
 * @param {boolean} sawClickAncestorAction A node during this search has a
 *     default action verb involving click ancestor or none.
 * @return {boolean}
 */
const isActionableOrHasActionableDescendant = function(
    node, sawClickAncestorAction = false) {
  // Static text nodes are never actionable for the purposes of navigation even
  // if they have default action verb set.
  if (node.role !== Role.STATIC_TEXT && node.defaultActionVerb &&
      (node.defaultActionVerb !==
           chrome.automation.DefaultActionVerb.CLICK_ANCESTOR ||
       sawClickAncestorAction)) {
    return true;
  }

  if (node.clickable) {
    return true;
  }

  sawClickAncestorAction = sawClickAncestorAction || !node.defaultActionVerb ||
      node.defaultActionVerb ===
          chrome.automation.DefaultActionVerb.CLICK_ANCESTOR;
  for (let i = 0; i < node.children.length; i++) {
    if (isActionableOrHasActionableDescendant(
            node.children[i], sawClickAncestorAction)) {
      return true;
    }
  }

  return false;
};

/**
 * A helper to check if any descendants of |node| are actionable.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
const hasActionableDescendant = function(node) {
  const sawClickAncestorAction = !node.defaultActionVerb ||
      node.defaultActionVerb ===
          chrome.automation.DefaultActionVerb.CLICK_ANCESTOR;
  for (let i = 0; i < node.children.length; i++) {
    if (isActionableOrHasActionableDescendant(
            node.children[i], sawClickAncestorAction)) {
      return true;
    }
  }

  return false;
};

/**
 * A helper to determine whether the children of a node are all
 * STATIC_TEXT, and whether the joined names of such children nodes are equal to
 * the current nodes name.
 * @param {!AutomationNode} node
 * @return {boolean}
 * @private
 */
const nodeNameContainedInStaticTextChildren = function(node) {
  const name = node.name;
  let child = node.firstChild;
  if (name === undefined || !child) {
    return false;
  }
  let nameIndex = 0;
  do {
    if (child.role !== Role.STATIC_TEXT) {
      return false;
    }
    if (name.substring(nameIndex, nameIndex + child.name.length) !==
        child.name) {
      return false;
    }
    nameIndex += child.name.length;
    // Either space or empty (i.e. end of string).
    const char = name.substring(nameIndex, nameIndex + 1);
    child = child.nextSibling;
    if ((child && char !== ' ') || char !== '') {
      return false;
    }
    nameIndex++;
  } while (child);
  return true;
};

export class AutomationPredicate {
  /**
   * Constructs a predicate given a list of roles.
   * @param {!Array<Role>} roles
   * @return {!AutomationPredicate.Unary}
   */
  static roles(roles) {
    return AutomationPredicate.match({anyRole: roles});
  }

  /**
   * Constructs a predicate given a list of roles or predicates.
   * @param {{anyRole: (Array<Role>|undefined),
   *          anyPredicate: (Array<AutomationPredicate.Unary>|undefined),
   *          anyAttribute: (Object|undefined)}} params
   * @return {!AutomationPredicate.Unary}
   */
  static match(params) {
    const anyRole = params.anyRole || [];
    const anyPredicate = params.anyPredicate || [];
    const anyAttribute = params.anyAttribute || {};
    return function(node) {
      return anyRole.some(function(role) {
        return role === node.role;
      }) ||
          anyPredicate.some(function(p) {
            return p(node);
          }) ||
          Object.keys(anyAttribute).some(function(key) {
            return node[key] === anyAttribute[key];
          });
    };
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static button(node) {
    return node.isButton;
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static comboBox(node) {
    return node.isComboBox;
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static checkBox(node) {
    return node.isCheckBox;
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static editText(node) {
    return node.role === Role.TEXT_FIELD ||
        (node.state[State.EDITABLE] && Boolean(node.parent) &&
         !node.parent.state[State.EDITABLE]);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static image(node) {
    return node.isImage && Boolean(node.name || node.url);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static visitedLink(node) {
    return node.state[State.VISITED];
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static focused(node) {
    return node.state[State.FOCUSED];
  }

  /**
   * Returns true if this node should be considered a leaf for touch
   * exploration.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static touchLeaf(node) {
    return Boolean(!node.firstChild && node.name) ||
        node.role === Role.BUTTON || node.role === Role.CHECK_BOX ||
        node.role === Role.POP_UP_BUTTON || node.role === Role.PORTAL ||
        node.role === Role.RADIO_BUTTON || node.role === Role.SLIDER ||
        node.role === Role.SWITCH || node.role === Role.TEXT_FIELD ||
        node.role === Role.TEXT_FIELD_WITH_COMBO_BOX ||
        (node.role === Role.MENU_ITEM && !hasActionableDescendant(node)) ||
        AutomationPredicate.image(node) ||
        // Simple list items should be leaves.
        AutomationPredicate.simpleListItem(node);
  }

  /**
   * Returns true if this node is marked as invalid.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static isInvalid(node) {
    return node.invalidState === InvalidState.TRUE ||
        AutomationPredicate.hasInvalidGrammarMarker(node) ||
        AutomationPredicate.hasInvalidSpellingMarker(node);
  }


  /**
   * Returns true if this node has an invalid grammar marker.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static hasInvalidGrammarMarker(node) {
    const markers = node.markers;
    if (!markers) {
      return false;
    }
    return markers.some(function(marker) {
      return marker.flags[MarkerType.GRAMMAR];
    });
  }

  /**
   * Returns true if this node has an invalid spelling marker.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static hasInvalidSpellingMarker(node) {
    const markers = node.markers;
    if (!markers) {
      return false;
    }
    return markers.some(function(marker) {
      return marker.flags[MarkerType.SPELLING];
    });
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leaf(node) {
    return Boolean(
        AutomationPredicate.touchLeaf(node) || node.role === Role.LIST_BOX ||
        // A node acting as a label should be a leaf if it has no actionable
        // controls.
        (node.labelFor && node.labelFor.length > 0 &&
         !isActionableOrHasActionableDescendant(node)) ||
        (node.descriptionFor && node.descriptionFor.length > 0 &&
         !isActionableOrHasActionableDescendant(node)) ||
        (node.activeDescendantFor && node.activeDescendantFor.length > 0) ||
        node.state[State.INVISIBLE] || node.children.every(function(n) {
          return n.state[State.INVISIBLE];
        }) ||
        AutomationPredicate.math(node));
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafWithText(node) {
    return AutomationPredicate.leaf(node) && Boolean(node.name || node.value);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafWithWordStop(node) {
    function hasWordStop(node) {
      if (node.role === Role.INLINE_TEXT_BOX) {
        return node.wordStarts && node.wordStarts.length;
      }

      // Non-text objects  are treated as having a single word stop.
      return true;
    }
    // Do not include static text leaves, which occur for an en end-of-line.
    return AutomationPredicate.leaf(node) && !node.state[State.INVISIBLE] &&
        node.role !== Role.STATIC_TEXT && hasWordStop(node);
  }

  /**
   * Matches against leaves or static text nodes. Useful when restricting
   * traversal to non-inline textboxes while still allowing them if navigation
   * already entered into an inline textbox.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static leafOrStaticText(node) {
    return AutomationPredicate.leaf(node) || node.role === Role.STATIC_TEXT;
  }

  /**
   * Matches against nodes visited during object navigation. An object as
   * defined below, are all nodes that are focusable or static text. When used
   * in tree walking, it should visit all nodes that tab traversal would as well
   * as non-focusable static text.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static object(node) {
    // Editable nodes are within a text-like field and don't make sense when
    // performing object navigation. Users should use line, word, or character
    // navigation. Only navigate to the top level node.
    if (node.parent && node.parent.state[State.EDITABLE] &&
        !node.parent.state[State.RICHLY_EDITABLE]) {
      return false;
    }

    // Things explicitly marked clickable (used only on ARC++) should be
    // visited.
    if (node.clickable) {
      return true;
    }

    // Given no other information, we want to visit focusable
    // (e.g. tabindex=0) nodes only when it has a name or is a control.
    if (node.state[State.FOCUSABLE] &&
        (node.name || node.state[State.EDITABLE] ||
         AutomationPredicate.formField(node))) {
      return true;
    }

    // Containers who have name from contents should be treated like objects if
    // the contents is all static text and not large.
    if (node.name && node.nameFrom === 'contents') {
      let onlyStaticText = true;
      let textLength = 0;
      for (let i = 0, child; child = node.children[i]; i++) {
        if (child.role !== Role.STATIC_TEXT) {
          onlyStaticText = false;
          break;
        }
        textLength += child.name ? child.name.length : 0;
      }

      if (onlyStaticText && textLength > 0 &&
          textLength < constants.OBJECT_MAX_CHARCOUNT) {
        return true;
      }
    }

    // Otherwise, leaf or static text nodes that don't contain only whitespace
    // should be visited with the exception of non-text only nodes. This covers
    // cases where an author might make a link with a name of ' '.
    return AutomationPredicate.leafOrStaticText(node) &&
        (/\S+/.test(node.name) ||
         (node.role !== Role.LINE_BREAK && node.role !== Role.STATIC_TEXT &&
          node.role !== Role.INLINE_TEXT_BOX));
  }

  /**
   * Matches against nodes visited during touch exploration.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static touchObject(node) {
    // Exclude large objects such as containers.
    if (AutomationPredicate.container(node)) {
      return false;
    }

    return AutomationPredicate.object(node);
  }

  /**
   * Matches against nodes visited during object navigation with a gesture.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static gestureObject(node) {
    if (node.role === Role.LIST_BOX) {
      return false;
    }
    return AutomationPredicate.object(node);
  }


  /**
   * @param {!AutomationNode} first
   * @param {!AutomationNode} second
   * @return {boolean}
   */
  static linebreak(first, second) {
    if (first.nextOnLine === second) {
      return false;
    }

    const fl = first.unclippedLocation;
    const sl = second.unclippedLocation;
    return fl.top !== sl.top || (fl.top + fl.height !== sl.top + sl.height);
  }

  /**
   * Matches against a node that contains other interesting nodes.
   * These nodes should always have their subtrees scanned when navigating.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static container(node) {
    // Math is never a container.
    if (AutomationPredicate.math(node)) {
      return false;
    }

    // Sometimes a focusable node will have a static text child with the same
    // name. During object navigation, the child will receive focus, resulting
    // in the name being read out twice.
    if (node.state[State.FOCUSABLE] &&
        nodeNameContainedInStaticTextChildren(node)) {
      return false;
    }
    // Do not consider containers that are clickable containers, unless they
    // also contain actionable nodes.
    if (node.clickable && !hasActionableDescendant(node)) {
      return false;
    }

    // Always try to dive into subtrees with actionable descendants for some
    // roles even if these roles are not naturally containers.
    if ([
          Role.BUTTON,
          Role.CELL,
          Role.CHECK_BOX,
          Role.RADIO_BUTTON,
          Role.SWITCH,
        ].includes(node.role) &&
        hasActionableDescendant(node)) {
      return true;
    }

    // Simple list items are not containers.
    if (AutomationPredicate.simpleListItem(node)) {
      return false;
    }

    return AutomationPredicate.match({
      anyRole: [
        Role.GENERIC_CONTAINER,
        Role.DOCUMENT,
        Role.GROUP,
        Role.LIST,
        Role.LIST_ITEM,
        Role.TAB,
        Role.TAB_PANEL,
        Role.TOOLBAR,
        Role.WINDOW,
      ],
      anyPredicate: [
        AutomationPredicate.landmark,
        AutomationPredicate.structuralContainer,
        function(node) {
          // For example, crosh.
          return node.role === Role.TEXT_FIELD &&
              node.restriction === Restriction.READ_ONLY;
        },
        function(node) {
          return (
              node.state[State.EDITABLE] && node.parent &&
              !node.parent.state[State.EDITABLE]);
        },
      ],
    })(node);
  }

  /**
   * Returns whether the given node should not be crossed when performing
   * traversals up the ancestry chain.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  static root(node) {
    if (node.modal) {
      return true;
    }

    switch (node.role) {
      case Role.WINDOW:
        return true;
      case Role.DIALOG:
        if (node.root.role !== Role.DESKTOP) {
          return Boolean(node.modal);
        }

        // The below logic handles nested dialogs properly in the desktop tree
        // like that found in a bubble view.
        return Boolean(node.parent) && node.parent.role === Role.WINDOW &&
            node.parent.children.every(function(child) {
              return node.role === Role.WINDOW || node.role === Role.DIALOG;
            });
      case Role.TOOLBAR:
        return node.root.role === Role.DESKTOP &&
            !(node.nextWindowFocus || !node.previousWindowFocus);
      case Role.ROOT_WEB_AREA:
        if (node.parent && node.parent.role === Role.WEB_VIEW &&
            !node.parent.state[State.FOCUSED]) {
          // If parent web view is not focused, we should allow this root web
          // area to be crossed when performing traversals up the ancestry
          // chain.
          return false;
        }
        return !node.parent || !node.parent.root ||
            (node.parent.root.role === Role.DESKTOP &&
             node.parent.role === Role.WEB_VIEW);
      default:
        return false;
    }
  }

  /**
   * Returns whether the given node should not be crossed when performing
   * traversal inside of an editable. Note that this predicate should not be
   * applied everywhere since there would be no way for a user to exit the
   * editable.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  static rootOrEditableRoot(node) {
    return AutomationPredicate.root(node) ||
        (node.state[State.RICHLY_EDITABLE] && node.state[State.FOCUSED] &&
         node.children.length > 0);
  }

  /**
   * Nodes that should be ignored while traversing the automation tree. For
   * example, apply this predicate when moving to the next object.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static shouldIgnoreNode(node) {
    // Ignore invisible nodes.
    if (node.state[State.INVISIBLE] ||
        (node.location.height === 0 && node.location.width === 0)) {
      return true;
    }

    // Ignore structural containres.
    if (AutomationPredicate.structuralContainer(node)) {
      return true;
    }

    // Ignore nodes acting as labels for another control, that are unambiguously
    // labels.
    if (node.labelFor && node.labelFor.length > 0 &&
        node.role === Role.LABEL_TEXT) {
      return true;
    }

    // Similarly, ignore nodes acting as descriptions.
    if (node.descriptionFor && node.descriptionFor.length > 0 &&
        node.role === Role.LABEL_TEXT) {
      return true;
    }

    // Ignore list markers that are followed by a static text.
    // The bullet will be added before the static text (or static text's inline
    // text box) in output.js.
    if (node.role === Role.LIST_MARKER && node.nextSibling &&
        node.nextSibling.role === Role.STATIC_TEXT) {
      return true;
    }

    // Don't ignore nodes with names or name-like attribute.
    if (node.name || node.value || node.description || node.url) {
      return false;
    }

    // Don't ignore math nodes.
    if (AutomationPredicate.math(node)) {
      return false;
    }

    // Ignore some roles.
    return AutomationPredicate.leaf(node) && (AutomationPredicate.roles([
             Role.CLIENT,
             Role.COLUMN,
             Role.GENERIC_CONTAINER,
             Role.GROUP,
             Role.IMAGE,
             Role.PARAGRAPH,
             Role.SCROLL_VIEW,
             Role.STATIC_TEXT,
             Role.SVG_ROOT,
             Role.TABLE_HEADER_CONTAINER,
             Role.UNKNOWN,
           ])(node));
  }

  /**
   * Returns if the node has a meaningful checked state.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static checkable(node) {
    return Boolean(node.checked);
  }

  /**
   * Returns a predicate that will match against the directed next cell taking
   * into account the current ancestor cell's position in the table.
   * @param {AutomationNode} start
   * @param {{dir: (constants.Dir|undefined),
   *           row: (boolean|undefined),
   *          col: (boolean|undefined)}} opts
   * |dir|, specifies direction for |row or/and |col| movement by one cell.
   *     |dir| defaults to forward.
   *     |row| and |col| are both false by default.
   *     |end| defaults to false. If set to true, |col| must also be set to
   * true. It will then return the first or last cell in the current column.
   * @return {?AutomationPredicate.Unary} Returns null if not in a table.
   */
  static makeTableCellPredicate(start, opts) {
    if (!opts.row && !opts.col) {
      throw new Error('You must set either row or col to true');
    }

    const dir = opts.dir || constants.Dir.FORWARD;

    // Compute the row/col index defaulting to 0.
    let rowIndex = 0;
    let colIndex = 0;
    let tableNode = start;
    while (tableNode) {
      if (AutomationPredicate.table(tableNode)) {
        break;
      }

      if (AutomationPredicate.cellLike(tableNode)) {
        rowIndex = tableNode.tableCellRowIndex;
        colIndex = tableNode.tableCellColumnIndex;
      }

      tableNode = tableNode.parent;
    }
    if (!tableNode) {
      return null;
    }

    // Only support making a predicate for column ends.
    if (opts.end) {
      if (!opts.col) {
        throw 'Unsupported option.';
      }

      if (dir === constants.Dir.FORWARD) {
        return function(node) {
          return AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex === colIndex &&
              node.tableCellRowIndex >= 0;
        };
      } else {
        return function(node) {
          return AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex === colIndex &&
              node.tableCellRowIndex < tableNode.tableRowCount;
        };
      }
    }

    // Adjust for the next/previous row/col.
    if (opts.row) {
      rowIndex = dir === constants.Dir.FORWARD ? rowIndex + 1 : rowIndex - 1;
    }
    if (opts.col) {
      colIndex = dir === constants.Dir.FORWARD ? colIndex + 1 : colIndex - 1;
    }

    return function(node) {
      return AutomationPredicate.cellLike(node) &&
          node.tableCellColumnIndex === colIndex &&
          node.tableCellRowIndex === rowIndex;
    };
  }

  /**
   * Returns a predicate that will match against a heading with a specific
   * hierarchical level.
   * @param {number} level 1-6
   * @return {AutomationPredicate.Unary}
   */
  static makeHeadingPredicate(level) {
    return function(node) {
      return node.role === Role.HEADING && node.hierarchicalLevel === level;
    };
  }

  /**
   * Matches against a node that forces showing surrounding contextual
   * information for braille.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static contextualBraille(node) {
    return node.parent != null &&
        ((node.parent.role === Role.ROW &&
          AutomationPredicate.cellLike(node)) ||
         (node.parent.role === Role.TREE &&
          node.parent.state[State.HORIZONTAL]));
  }

  /**
   * Matches against a node that handles multi line key commands.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static multiline(node) {
    return node.state[State.MULTILINE] || node.state[State.RICHLY_EDITABLE];
  }

  /**
   * Matches against a node that should be auto-scrolled during navigation.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static autoScrollable(node) {
    return Boolean(node.scrollable) &&
        (node.standardActions.includes(
             chrome.automation.ActionType.SCROLL_FORWARD) ||
         node.standardActions.includes(
             chrome.automation.ActionType.SCROLL_BACKWARD)) &&
        (node.role === Role.GRID || node.role === Role.LIST ||
         node.role === Role.POP_UP_BUTTON || node.role === Role.SCROLL_VIEW);
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static math(node) {
    return node.role === Role.MATH ||
        Boolean(node.htmlAttributes['data-mathml']);
  }

  /**
   * Matches against nodes visited during group navigation.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static group(node) {
    if (AutomationPredicate.text(node) || node.display === 'inline') {
      return false;
    }

    return AutomationPredicate.match({
      anyRole: [Role.HEADING, Role.LIST, Role.PARAGRAPH],
      anyPredicate: [
        AutomationPredicate.editText,
        AutomationPredicate.formField,
        AutomationPredicate.object,
        AutomationPredicate.table,
      ],
    })(node);
  }

  /**
   * Matches against editable nodes, that should not be treated in the usual
   * fashion.
   * Instead, only output the contents around the selection in braille.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static shouldOnlyOutputSelectionChangeInBraille(node) {
    return node.state[State.RICHLY_EDITABLE] && node.state[State.FOCUSED] &&
        node.role === Role.LOG;
  }

  /**
   * Matches against nodes we should ignore in a jump command.
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  static ignoreDuringJump(node) {
    return node.role === Role.GENERIC_CONTAINER ||
        node.role === Role.STATIC_TEXT || node.role === Role.INLINE_TEXT_BOX;
  }

  /**
   * Returns a predicate that will match against a list-like node. The returned
   * predicate should not match the first list-like ancestor of |node| (or
   * |node| itself, if it is list-like).
   * @param {AutomationNode} node
   * @return {AutomationPredicate.Unary}
   */
  static makeListPredicate(node) {
    // Scan upward for a list-like ancestor. We do not want to match against
    // this node.
    let avoidNode = node;
    while (avoidNode && !AutomationPredicate.listLike(avoidNode)) {
      avoidNode = avoidNode.parent;
    }

    return function(autoNode) {
      return AutomationPredicate.listLike(autoNode) && (autoNode !== avoidNode);
    };
  }
}

/**
 * @typedef {function(!AutomationNode) : boolean}
 */
AutomationPredicate.Unary;

/**
 * @typedef {function(!AutomationNode,
 * !AutomationNode) : boolean}
 */
AutomationPredicate.Binary;


/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading = AutomationPredicate.roles([Role.HEADING]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.roles([Role.INLINE_TEXT_BOX]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.roles([Role.LINK]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.row = AutomationPredicate.roles([Role.ROW]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table =
    AutomationPredicate.roles([Role.GRID, Role.LIST_GRID, Role.TABLE]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.listLike =
    AutomationPredicate.roles([Role.LIST, Role.DESCRIPTION_LIST]);

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.simpleListItem = AutomationPredicate.match({
  anyPredicate:
      [node => node.role === Role.LIST_ITEM && node.children.length === 2 &&
           node.firstChild.role === Role.LIST_MARKER &&
           node.lastChild.role === Role.STATIC_TEXT],
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.formField = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.button,
    AutomationPredicate.comboBox,
    AutomationPredicate.editText,
  ],
  anyRole: [
    Role.CHECK_BOX,
    Role.COLOR_WELL,
    Role.LIST_BOX,
    Role.SLIDER,
    Role.SWITCH,
    Role.TAB,
    Role.TREE,
  ],
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.control = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.formField,
  ],
  anyRole: [
    Role.DISCLOSURE_TRIANGLE,
    Role.MENU_ITEM,
    Role.MENU_ITEM_CHECK_BOX,
    Role.MENU_ITEM_RADIO,
    Role.MENU_LIST_OPTION,
    Role.SCROLL_BAR,
  ],
});


/** @type {AutomationPredicate.Unary} */
AutomationPredicate.linkOrControl = AutomationPredicate.match(
    {anyPredicate: [AutomationPredicate.control], anyRole: [Role.LINK]});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.landmark = AutomationPredicate.roles([
  Role.APPLICATION,
  Role.BANNER,
  Role.COMPLEMENTARY,
  Role.CONTENT_INFO,
  Role.FORM,
  Role.MAIN,
  Role.NAVIGATION,
  Role.REGION,
  Role.SEARCH,
]);

/**
 * Matches against nodes that contain interesting nodes, but should never be
 * visited.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.structuralContainer = AutomationPredicate.roles([
  Role.ALERT_DIALOG,
  Role.CLIENT,
  Role.DIALOG,
  Role.LAYOUT_TABLE,
  Role.LAYOUT_TABLE_CELL,
  Role.LAYOUT_TABLE_ROW,
  Role.ROOT_WEB_AREA,
  Role.WEB_VIEW,
  Role.WINDOW,
  Role.EMBEDDED_OBJECT,
  Role.IFRAME,
  Role.IFRAME_PRESENTATIONAL,
  Role.PLUGIN_OBJECT,
  Role.UNKNOWN,
  Role.PANE,
  Role.SCROLL_VIEW,
]);


/**
 * Returns if the node is clickable.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.clickable = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.button,
    AutomationPredicate.link,
    node => {
      return node.defaultActionVerb ===
          chrome.automation.DefaultActionVerb.CLICK;
    },
  ],
  anyAttribute: {clickable: true},
});

/**
 * Returns if the node is long clickable.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.longClickable = AutomationPredicate.match({
  anyPredicate: [
    node => {
      return node.standardActions.includes(
          chrome.automation.ActionType.LONG_CLICK);
    },
  ],
  anyAttribute: {longClickable: true},
});

// Table related predicates.
/**
 * Returns if the node has a cell like role.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.cellLike =
    AutomationPredicate.roles([Role.CELL, Role.ROW_HEADER, Role.COLUMN_HEADER]);


/**
 * Matches against nodes that we may be able to retrieve image data from.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.supportsImageData =
    AutomationPredicate.roles([Role.CANVAS, Role.IMAGE, Role.VIDEO]);


/**
 * Matches against menu item like nodes.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.menuItem = AutomationPredicate.roles(
    [Role.MENU_ITEM, Role.MENU_ITEM_CHECK_BOX, Role.MENU_ITEM_RADIO]);

/**
 * Matches against text like nodes.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.text = AutomationPredicate.roles(
    [Role.STATIC_TEXT, Role.INLINE_TEXT_BOX, Role.LINE_BREAK]);

/**
 * Matches against selecteable text like nodes.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.selectableText = AutomationPredicate.roles([
  Role.STATIC_TEXT,
  Role.INLINE_TEXT_BOX,
  Role.LINE_BREAK,
  Role.LIST_MARKER,
]);

/**
 * Matches against pop-up button like nodes.
 * Historically, single value <select> controls were represented as a
 * popup button, but they are distinct from <button aria-haspopup='menu'>.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.popUpButton = AutomationPredicate.roles([
  Role.COMBO_BOX_SELECT,
  Role.POP_UP_BUTTON,
]);
