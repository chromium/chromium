// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Predicates for the automation extension API.
 */
import {constants} from './constants.js';
import {TestImportManager} from './testing/test_import_manager.js';

import ActionType = chrome.automation.ActionType;
import AutomationNode = chrome.automation.AutomationNode;
import DefaultActionVerb = chrome.automation.DefaultActionVerb;
import Dir = constants.Dir;
import InvalidState = chrome.automation.InvalidState;
import MarkerType = chrome.automation.MarkerType;
import Restriction = chrome.automation.Restriction;
import Role = chrome.automation.RoleType;
import State = chrome.automation.StateType;

interface MatchParams {
  anyRole?: Role[];
  anyPredicate?: AutomationPredicate.Unary[];
}

interface TableCellPredicateOptions {
  dir?: Dir;
  end?: boolean;
  row?: boolean;
  col?: boolean;
}

/**
 * A helper to check if |node| or any descendant is actionable.
 * @param sawClickAncestorAction A node during this search has a
 *     default action verb involving click ancestor or none.
 */
const isActionableOrHasActionableDescendant = function(
    node: AutomationNode, sawClickAncestorAction: boolean = false): boolean {
  // Static text nodes are never actionable for the purposes of navigation even
  // if they have default action verb set.
  if (node.role !== Role.STATIC_TEXT && node.defaultActionVerb &&
      (node.defaultActionVerb !== DefaultActionVerb.CLICK_ANCESTOR ||
       sawClickAncestorAction)) {
    return true;
  }

  if (node.clickable) {
    return true;
  }

  sawClickAncestorAction = sawClickAncestorAction || !node.defaultActionVerb ||
      node.defaultActionVerb === DefaultActionVerb.CLICK_ANCESTOR;
  for (let i = 0; i < node.children.length; i++) {
    if (isActionableOrHasActionableDescendant(
            node.children[i], sawClickAncestorAction)) {
      return true;
    }
  }

  return false;
};

/** A helper to check if any descendants of |node| are actionable. */
const hasActionableDescendant = function(node: AutomationNode): boolean {
  const sawClickAncestorAction = !node.defaultActionVerb ||
      node.defaultActionVerb === DefaultActionVerb.CLICK_ANCESTOR;
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
 */
const nodeNameContainedInStaticTextChildren = function(
    node: AutomationNode): boolean {
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
    if (child.name === undefined) {
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

export namespace AutomationPredicate {
  /** Constructs a predicate given a list of roles. */
  export function roles(roles: Role[]): AutomationPredicate.Unary {
    return AutomationPredicate.match({anyRole: roles});
  }

  /** Constructs a predicate given a list of roles or predicates. */
  export function match(params: MatchParams): AutomationPredicate.Unary {
    const anyRole = params.anyRole || [];
    const anyPredicate = params.anyPredicate || [];
    return function(node: AutomationNode): boolean {
      return anyRole.some(role => role === node.role) ||
          anyPredicate.some(p => p(node));
    };
  }

  export function button(node: AutomationNode): boolean {
    return node.isButton;
  }

  export function comboBox(node: AutomationNode): boolean {
    return node.isComboBox;
  }

  export function checkBox(node: AutomationNode): boolean {
    return node.isCheckBox;
  }

  export function editText(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return node.role === Role.TEXT_FIELD ||
        (node.state![State.EDITABLE] && Boolean(node.parent) &&
         !node.parent!.state![State.EDITABLE]);
  }

  export function image(node: AutomationNode): boolean {
    return node.isImage && Boolean(node.name || node.url);
  }

  export function visitedLink(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
   return node.state![State.VISITED];
  }

  export function focused(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return node.state![State.FOCUSED];
  }

  /**
   * Returns true if this node should be considered a leaf for touch
   * exploration.
   */
  export function touchLeaf(node: AutomationNode): boolean {
    return Boolean(!node.firstChild && node.name) ||
        node.role === Role.BUTTON || node.role === Role.CHECK_BOX ||
        node.role === Role.POP_UP_BUTTON || node.role === Role.RADIO_BUTTON ||
        node.role === Role.SLIDER || node.role === Role.SWITCH ||
        node.role === Role.TEXT_FIELD ||
        node.role === Role.TEXT_FIELD_WITH_COMBO_BOX ||
        (node.role === Role.MENU_ITEM && !hasActionableDescendant(node)) ||
        AutomationPredicate.image(node) ||
        // Simple list items should be leaves.
        AutomationPredicate.simpleListItem(node);
  }

  /** Returns true if this node is marked as invalid. */
  export function isInvalid(node: AutomationNode): boolean {
    return node.invalidState === InvalidState.TRUE ||
        AutomationPredicate.hasInvalidGrammarMarker(node) ||
        AutomationPredicate.hasInvalidSpellingMarker(node);
  }

  /** Returns true if this node has an invalid grammar marker. */
  export function hasInvalidGrammarMarker(node: AutomationNode): boolean {
    const markers = node.markers;
    if (!markers) {
      return false;
    }
    return markers.some(function(marker) {
      return marker.flags[MarkerType.GRAMMAR];
    });
  }

  /** Returns true if this node has an invalid spelling marker. */
  export function hasInvalidSpellingMarker(node: AutomationNode): boolean {
    const markers = node.markers;
    if (!markers) {
      return false;
    }
    return markers.some(function(marker) {
      return marker.flags[MarkerType.SPELLING];
    });
  }

  export function leaf(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return Boolean(
        AutomationPredicate.touchLeaf(node) || node.role === Role.LIST_BOX ||
        // A node acting as a label should be a leaf if it has no actionable
        // controls.
        (node.labelFor && node.labelFor.length > 0 &&
         !isActionableOrHasActionableDescendant(node)) ||
        (node.descriptionFor && node.descriptionFor.length > 0 &&
         !isActionableOrHasActionableDescendant(node)) ||
        (node.activeDescendantFor && node.activeDescendantFor.length > 0) ||
        node.state![State.INVISIBLE] ||
        node.children.every((n: AutomationNode) => n.state![State.INVISIBLE]) ||
        AutomationPredicate.math(node));
  }

  export function leafWithText(node: AutomationNode): boolean {
    return AutomationPredicate.leaf(node) && Boolean(node.name || node.value);
  }

  export function leafWithWordStop(node: AutomationNode): boolean {
    function hasWordStop(node: AutomationNode): boolean {
      if (node.role === Role.INLINE_TEXT_BOX) {
        return Boolean(node.wordStarts && node.wordStarts.length);
      }

      // Non-text objects  are treated as having a single word stop.
      return true;
    }
    // Do not include static text leaves, which occur for an en end-of-line.
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return AutomationPredicate.leaf(node) && !node.state![State.INVISIBLE] &&
        node.role !== Role.STATIC_TEXT && hasWordStop(node);
  }

  /**
   * Matches against leaves or static text nodes. Useful when restricting
   * traversal to non-inline textboxes while still allowing them if navigation
   * already entered into an inline textbox.
   */
  export function leafOrStaticText(node: AutomationNode): boolean {
    return AutomationPredicate.leaf(node) || node.role === Role.STATIC_TEXT;
  }

  /**
   * Matches against nodes visited during object navigation. An object as
   * defined below, are all nodes that are focusable or static text. When used
   * in tree walking, it should visit all nodes that tab traversal would as well
   * as non-focusable static text.
   */
  export function object(node: AutomationNode): boolean {
    // Editable nodes are within a text-like field and don't make sense when
    // performing object navigation. Users should use line, word, or character
    // navigation. Only navigate to the top level node.
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (node.parent && node.parent.state![State.EDITABLE] &&
        !node.parent.state![State.RICHLY_EDITABLE]) {
      return false;
    }

    // Things explicitly marked clickable (used only on ARC++) should be
    // visited.
    if (node.clickable) {
      return true;
    }

    // Given no other information, we want to visit focusable
    // (e.g. tabindex=0) nodes only when it has a name or is a control.
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (node.state![State.FOCUSABLE] &&
        (node.name || node.state![State.EDITABLE] ||
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
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return AutomationPredicate.leafOrStaticText(node) &&
        (/\S+/.test(node.name!) ||
         (node.role !== Role.LINE_BREAK && node.role !== Role.STATIC_TEXT &&
          node.role !== Role.INLINE_TEXT_BOX));
  }

  /** Matches against nodes visited during touch exploration. */
  export function touchObject(node: AutomationNode): boolean {
    // Exclude large objects such as containers.
    if (AutomationPredicate.container(node)) {
      return false;
    }

    return AutomationPredicate.object(node);
  }

  /** Matches against nodes visited during object navigation with a gesture. */
  export function gestureObject(node: AutomationNode): boolean {
    if (node.role === Role.LIST_BOX) {
      return false;
    }
    return AutomationPredicate.object(node);
  }

  export function linebreak(
      first: AutomationNode, second: AutomationNode): boolean {
    if (first.nextOnLine === second) {
      return false;
    }

    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    const fl = first.unclippedLocation!;
    const sl = second.unclippedLocation!;
    return fl.top !== sl.top || (fl.top + fl.height !== sl.top + sl.height);
  }

  /**
   * Matches against a node that contains other interesting nodes.
   * These nodes should always have their subtrees scanned when navigating.
   */
  export function container(node: AutomationNode): boolean {
    // Math is never a container.
    if (AutomationPredicate.math(node)) {
      return false;
    }

    // Sometimes a focusable node will have a static text child with the same
    // name. During object navigation, the child will receive focus, resulting
    // in the name being read out twice.
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (node.state![State.FOCUSABLE] &&
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
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if ([
          Role.BUTTON,
          Role.CELL,
          Role.CHECK_BOX,
          Role.GRID_CELL,
          Role.RADIO_BUTTON,
          Role.SWITCH,
        ].includes(node.role!) &&
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
        Role.PDF_ROOT,
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
        (node: AutomationNode) => {
          // For example, crosh.
          return node.role === Role.TEXT_FIELD &&
              node.restriction === Restriction.READ_ONLY;
        },
        // TODO(b/314203187): Not null asserted, check to make sure it's
        // correct.
        (node: AutomationNode) => (
              node.state![State.EDITABLE] && node.parent &&
              !node.parent.state![State.EDITABLE]),
      ],
    })(node);
  }

  /**
   * Returns whether the given node should not be crossed when performing
   * traversals up the ancestry chain.
   */
  export function root(node: AutomationNode): boolean {
    if (node.modal) {
      return true;
    }

    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    switch (node.role) {
      case Role.WINDOW:
        return true;
      case Role.DIALOG:
        if (node.root!.role !== Role.DESKTOP) {
          return Boolean(node.modal);
        }

        // The below logic handles nested dialogs properly in the desktop tree
        // like that found in a bubble view.
        return node.parent !== undefined && node.parent.role === Role.WINDOW &&
            node.parent.children.every((_child: AutomationNode) =>
                // TODO(b/322191528): This should be |child|, not |node|.
                node.role === Role.WINDOW || node.role === Role.DIALOG);
      case Role.TOOLBAR:
        return node.root!.role === Role.DESKTOP &&
            !(node.nextWindowFocus || !node.previousWindowFocus);
      case Role.ROOT_WEB_AREA:
        if (node.parent && node.parent.role === Role.WEB_VIEW &&
            !node.parent.state![State.FOCUSED]) {
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
   */
  export function rootOrEditableRoot(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return AutomationPredicate.root(node) ||
        (node.state![State.RICHLY_EDITABLE] && node.state![State.FOCUSED] &&
         node.children.length > 0);
  }

  /**
   * Nodes that should be ignored while traversing the automation tree. For
   * example, apply this predicate when moving to the next object.
   */
  export function shouldIgnoreNode(node: AutomationNode): boolean {
    // Ignore invisible nodes.
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    if (node.state![State.INVISIBLE] ||
        (node.location.height === 0 && node.location.width === 0)) {
      return true;
    }

    // Ignore structural containers.
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

    // AXTreeSourceAndroid computes names for clickables.
    // Ignore nodes for which this computation is not done
    if (node.clickable && !node.name && !node.value && !node.description) {
      return true;
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

  /** Returns if the node has a meaningful checked state. */
  export function checkable(node: AutomationNode): boolean {
    return Boolean(node.checked);
  }

  /**
   * Returns a predicate that will match against the directed next cell taking
   * into account the current ancestor cell's position in the table.
   * @param opts |dir|, specifies direction for |row or/and |col| movement by
   *     one cell. |dir| defaults to forward.
   * |row| and |col| are both false by default.
   * |end| defaults to false. If set to true, |col| must also be set to
   *     true. It will then return the first or last cell in the current column.
   * @return Returns null if not in a table.
   */
  export function makeTableCellPredicate(
      start: AutomationNode,
      opts: TableCellPredicateOptions): AutomationPredicate.Unary | null {
    if (!opts.row && !opts.col) {
      throw new Error('You must set either row or col to true');
    }

    const dir = opts.dir || Dir.FORWARD;

    // Compute the row/col index defaulting to 0.
    let rowIndex = 0;
    let colIndex = 0;
    let tableNode: AutomationNode | undefined = start;
    while (tableNode) {
      if (AutomationPredicate.table(tableNode)) {
        break;
      }

      // TODO(b/314203187): Not null asserted, check to make sure it's correct.
      if (AutomationPredicate.cellLike(tableNode)) {
        rowIndex = tableNode.tableCellRowIndex!;
        colIndex = tableNode.tableCellColumnIndex!;
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

      // TODO(b/314203187): Not null asserted, check to make sure it's correct.
      if (dir === Dir.FORWARD) {
        return (node: AutomationNode) => AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex === colIndex &&
              node.tableCellRowIndex! >= 0;
      } else {
        return (node: AutomationNode) => AutomationPredicate.cellLike(node) &&
              node.tableCellColumnIndex === colIndex &&
              node.tableCellRowIndex! < tableNode!.tableRowCount!;
      }
    }

    // Adjust for the next/previous row/col.
    if (opts.row) {
      rowIndex = dir === Dir.FORWARD ? rowIndex + 1 : rowIndex - 1;
    }
    if (opts.col) {
      colIndex = dir === Dir.FORWARD ? colIndex + 1 : colIndex - 1;
    }

    return (node: AutomationNode) => AutomationPredicate.cellLike(node) &&
          node.tableCellColumnIndex === colIndex &&
          node.tableCellRowIndex === rowIndex;
  }

  /**
   * Returns a predicate that will match against a heading with a specific
   * hierarchical level.
   * @param level 1-6
   */
  export function makeHeadingPredicate(
      level: number): AutomationPredicate.Unary {
    return function(node: AutomationNode) {
      return node.role === Role.HEADING && node.hierarchicalLevel === level;
    };
  }

  /**
   * Matches against a node that forces showing surrounding contextual
   * information for braille.
   */
  export function contextualBraille(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return node.parent != null &&
        ((node.parent.role === Role.ROW &&
          AutomationPredicate.cellLike(node)) ||
         (node.parent.role === Role.TREE &&
          node.parent.state![State.HORIZONTAL]));
  }

  /**
   * Matches against a node that handles multi line key commands.
   */
  export function multiline(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return node.state![State.MULTILINE] || node.state![State.RICHLY_EDITABLE];
  }

  /** Matches against a node that should be auto-scrolled during navigation. */
  export function autoScrollable(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return Boolean(node.scrollable) &&
        (node.standardActions!.includes(ActionType.SCROLL_FORWARD) ||
         node.standardActions!.includes(ActionType.SCROLL_BACKWARD)) &&
        (node.role === Role.GRID || node.role === Role.LIST ||
         node.role === Role.POP_UP_BUTTON || node.role === Role.SCROLL_VIEW);
  }

  export function math(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return Boolean(node.mathContent);
  }

  /** Matches against nodes visited during group navigation. */
  export function group(node: AutomationNode): boolean {
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
   */
  export function shouldOnlyOutputSelectionChangeInBraille(
      node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check to make sure it's correct.
    return node.state![State.RICHLY_EDITABLE] && node.state![State.FOCUSED] &&
        node.role === Role.LOG;
  }

  /** Matches against nodes we should ignore in a jump command. */
  export function ignoreDuringJump(node: AutomationNode): boolean {
    return node.role === Role.GENERIC_CONTAINER ||
        node.role === Role.STATIC_TEXT || node.role === Role.INLINE_TEXT_BOX;
  }

  /**
   * Returns a predicate that will match against a list-like node. The returned
   * predicate should not match the first list-like ancestor of |avoidThis| (or
   * |avoidThis| itself, if it is list-like).
   */
  export function makeListPredicate(
      avoidThis: AutomationNode): AutomationPredicate.Unary {
    // Scan upward for a list-like ancestor. We do not want to match against
    // this node.
    let avoidNode: AutomationNode | undefined = avoidThis;
    while (avoidNode && !AutomationPredicate.listLike(avoidNode)) {
      avoidNode = avoidNode.parent;
    }

    return function(node: AutomationNode): boolean {
      return AutomationPredicate.listLike(node) && (node !== avoidNode);
    };
  }

  export type Unary = (node: AutomationNode) => boolean;
  export type Binary =
      (first: AutomationNode, second: AutomationNode) => boolean;

  export const heading = AutomationPredicate.roles([Role.HEADING]);
  export const inlineTextBox =
      AutomationPredicate.roles([Role.INLINE_TEXT_BOX]);
  export const link = AutomationPredicate.roles([Role.LINK]);
  export const row = AutomationPredicate.roles([Role.ROW]);
  export const table =
      AutomationPredicate.roles([Role.GRID, Role.LIST_GRID, Role.TABLE]);
  export const listLike =
      AutomationPredicate.roles([Role.LIST, Role.DESCRIPTION_LIST]);

  // TODO(b/314203187): Not null asserted, check to make sure it's correct.
  export const simpleListItem = AutomationPredicate.match({
    anyPredicate:
        [node => node.role === Role.LIST_ITEM && node.children.length === 2 &&
            node.firstChild!.role === Role.LIST_MARKER &&
            node.lastChild!.role === Role.STATIC_TEXT],
  });

  export const formField = AutomationPredicate.match({
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

  export const control = AutomationPredicate.match({
    anyPredicate: [
      AutomationPredicate.formField,
    ],
    anyRole: [
      Role.DISCLOSURE_TRIANGLE,
      Role.MENU_ITEM,
      Role.MENU_ITEM_CHECK_BOX,
      Role.MENU_ITEM_RADIO,
      Role.SCROLL_BAR,
    ],
  });

  export const linkOrControl = AutomationPredicate.match(
      {anyPredicate: [AutomationPredicate.control], anyRole: [Role.LINK]});

  export const landmark = AutomationPredicate.roles([
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
   */
  export const structuralContainer = AutomationPredicate.roles([
    Role.ALERT_DIALOG,
    Role.CLIENT,
    Role.DIALOG,
    Role.LAYOUT_TABLE,
    Role.LAYOUT_TABLE_CELL,
    Role.LAYOUT_TABLE_ROW,
    Role.MENU_LIST_POPUP,
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

  export const clickable = AutomationPredicate.match({
    anyPredicate: [
      AutomationPredicate.button,
      AutomationPredicate.link,
      node => node.defaultActionVerb === DefaultActionVerb.CLICK,
      node => node.clickable === true,
    ],
  });

  // TODO(b/314203187): Not null asserted, check to make sure it's correct.
  export const longClickable = AutomationPredicate.match({
    anyPredicate: [
      node => node.standardActions!.includes(
          chrome.automation.ActionType.LONG_CLICK),
      // @ts-ignore Long clickable doesn't seem to be a property?
      node => node.longClickable === true,
    ],
  });

  /** Returns if the node is a list option, either in a menu or a listbox. */
  export const listOption =
      AutomationPredicate.roles([Role.LIST_BOX_OPTION, Role.MENU_LIST_OPTION]);

  // Table related predicates.
  /** Returns if the node has a cell like role. */
  export const cellLike = AutomationPredicate.roles(
      [Role.CELL, Role.GRID_CELL, Role.ROW_HEADER, Role.COLUMN_HEADER]);

  /** Returns if the node is a table header. */
  export const tableHeader =
      AutomationPredicate.roles([Role.ROW_HEADER, Role.COLUMN_HEADER]);

  /** Matches against nodes that we may be able to retrieve image data from. */
  export const supportsImageData =
      AutomationPredicate.roles([Role.CANVAS, Role.IMAGE, Role.VIDEO]);

  /** Matches against menu item like nodes. */
  export const menuItem = AutomationPredicate.roles(
      [Role.MENU_ITEM, Role.MENU_ITEM_CHECK_BOX, Role.MENU_ITEM_RADIO]);

  /** Matches against text like nodes. */
  export const text = AutomationPredicate.roles(
      [Role.STATIC_TEXT, Role.INLINE_TEXT_BOX, Role.LINE_BREAK]);

  /** Matches against selecteable text like nodes. */
  export const selectableText = AutomationPredicate.roles([
    Role.STATIC_TEXT,
    Role.INLINE_TEXT_BOX,
    Role.LINE_BREAK,
    Role.LIST_MARKER,
  ]);

  /**
   * Matches against pop-up button like nodes.
   * Historically, single value <select> controls were represented as a
   * popup button, but they are distinct from <button aria-haspopup='menu'>.
   */
  export const popUpButton = AutomationPredicate.roles([
    Role.COMBO_BOX_SELECT,
    Role.POP_UP_BUTTON,
  ]);
}

TestImportManager.exportForTesting(
    ['AutomationPredicate', AutomationPredicate]);
