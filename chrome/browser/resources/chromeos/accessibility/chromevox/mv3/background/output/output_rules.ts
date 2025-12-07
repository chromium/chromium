// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains the rules for output based on type information.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {AbstractRole, ChromeVoxRole, CustomRole} from '../../common/role_type.js';

import {OutputRoleInfo} from './output_role_info.js';
import {OutputCustomEvent, OutputEventType, OutputFormatType, OutputNavigationType} from './output_types.js';

const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

export interface OutputRuleSpecifier {
  event: string;
  role: string;
  navigation: string|undefined;
  output: string|undefined;
}
;

export class OutputRule {
  protected event_: OutputEventType;
  protected role_: ChromeVoxRole = CustomRole.DEFAULT;
  protected navigation_: OutputNavigationType|undefined;
  protected output_: OutputFormatType|undefined;

  constructor(event: OutputEventType) {
    this.event_ = this.getEvent_(event);
  }

  private getEvent_(event: OutputEventType): OutputEventType {
    if (OutputRule.RULES[event]) {
      return event;
    }
    return OutputCustomEvent.NAVIGATE;
  }

  get specifier(): OutputRuleSpecifier {
    return {
      event: this.event_,
      role: this.role_,
      navigation: this.navigation_,
      output: this.output_,
    };
  }

  get formatString(): string {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    return OutputRule.RULES[this.event_]![this.role_]![this.output_!]!;
  }

  /**
   * @return true if the role was set, false otherwise.
   */
  populateRole(
      role: ChromeVoxRole|undefined,
      formatName: OutputFormatType|OutputNavigationType|undefined): boolean {
    if (this.hasRule_(role, formatName) && role) {
      this.role_ = role;
      return true;
    }
    if (this.hasRule_(parent(role), formatName) &&
        parent(role) !== CustomRole.NO_ROLE) {
      this.role_ = parent(role);
      return true;
    }
    return false;
  }

  // The following setter functions are a temporary measure.
  // TODO(anastasi): move the logic for determining the below properties into
  // this class.

  set role(role: ChromeVoxRole) {
    this.role_ = role;
  }

  set output(output: OutputFormatType|undefined) {
    this.output_ = output;
  }

  get event(): OutputEventType {
    return this.event_;
  }
  get role(): ChromeVoxRole {
    return this.role_;
  }
  get navigation(): OutputNavigationType|undefined {
    return this.navigation_;
  }
  get output(): OutputFormatType|undefined {
    return this.output_;
  }

  // ========= Private methods =========

  /**
   * Returns true if there is a rule for this role/format combo.
   */
  private hasRule_(
      role: ChromeVoxRole|undefined,
      format: OutputFormatType|OutputNavigationType|undefined): boolean {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    const eventBlock = OutputRule.RULES[this.event_]!;
    return Boolean(
        role && eventBlock[role] && format && eventBlock[role][format]);
  }
}

export class AncestryOutputRule extends OutputRule {
  constructor(
      eventType: OutputEventType, role: ChromeVoxRole|undefined,
      navigationType: OutputNavigationType|undefined, tryBraille: boolean) {
    super(eventType);

    this.populateRole(role, navigationType);
    this.populateNavigation(navigationType);
    this.populateOutput(tryBraille);
  }

  populateNavigation(navigationType: OutputNavigationType|undefined) {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    if (navigationType && OutputRule.RULES[this.event_]![this.role_] &&
        OutputRule.RULES[this.event_]![this.role_]![navigationType]) {
      this.navigation_ = navigationType;
    }
  }

  populateOutput(tryBraille: boolean) {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    if (!OutputRule.RULES[this.event_]![this.role_]) {
      // Invalid rule case.
      return;
    }

    const rule = OutputRule.RULES[this.event_]![this.role_]![this.navigation_!];
    if (!rule || !isEnterRule(rule)) {
      return;
    }

    const enter = rule as EnterRule;
    if (enter.speak) {
      this.output_ = OutputFormatType.SPEAK;
    }
    if (tryBraille && enter.braille) {
      this.output_ = OutputFormatType.BRAILLE;
    }
  }

  get defined(): boolean {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    return Boolean(
        OutputRule.RULES[this.event_]![this.role_] &&
        OutputRule.RULES[this.event_]![this.role_]![this.navigation_!]);
  }

  get enterFormat(): string {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    const rule = OutputRule.RULES[this.event_]![this.role_]![this.navigation_!];
    if (this.output_ && isEnterRule(rule)) {
      return (rule as EnterRule)[this.output_]!;
    }
    return String(rule) || '';
  }
}

function parent(role: ChromeVoxRole|undefined): ChromeVoxRole {
  if (!role) {
    return CustomRole.NO_ROLE;
  }
  return OutputRoleInfo[role as chrome.automation.RoleType]?.inherits ??
      CustomRole.NO_ROLE;
}

/**
 * An object that specifies the rules for outputting a certain role on a
 * specific event, based on the type of output.
 *
 * speak: The speech rule for when ChromeVox range lands exactly on the node.
 * braille: The braille rule for when ChromeVox range lands exactly on the node.
 * enter: The rule for when ChromeVox range enters the node's subtree.
 *    Can contain speak and braille properties.
 * leave: The rule for when ChromeVox range exits the node's subtree.
 * startOf: The rule applied for each ancestor diff of a range and its previous
 * leaf range.
 * endOf: The rule applied for each ancestor diff of a range and its
 * next leaf range.
 */
interface OutputRuleDefinition {
  speak?: string;
  braille?: string;
  enter?: string|EnterRule;
  leave?: string;
  startOf?: string;
  endOf?: string;
}

interface EnterRule {
  speak?: string;
  braille?: string;
}

function isEnterRule(rule: any): rule is EnterRule {
  if (!rule || typeof rule !== 'object') {
    return false;
  }

  const keys = Object.keys(rule);
  if (keys.length > 2) {
    return false;
  }

  for (const key of keys) {
    if (key === 'speak' || key === 'braille') {
      continue;
    }
    return false;
  }

  return true;
}

export namespace OutputRule {
  /**
   * Rules specifying format of AutomationNodes for output.
   * Please see above for more information on properties.
   */
  export const RULES: Partial<Record<
      OutputEventType,
      Partial<Record<ChromeVoxRole, OutputRuleDefinition>>>> = {
    navigate: {
      [CustomRole.DEFAULT]: {
        speak: `$name $node(activeDescendant) $value $state $restriction $role
          $description`,
        braille: ``,
      },
      [AbstractRole.CONTAINER]: {
        startOf: `$nameFromNode $role $state $description`,
        endOf: `@end_of_container($role)`,
      },
      [AbstractRole.FORM_FIELD_CONTAINER]: {
        enter: `$nameFromNode $role $state $description`,
        leave: `@exited_container($role)`,
      },
      [AbstractRole.ITEM]: {
        // Note that ChromeVox generally does not output position/count. Only
        // for some roles (see sub-output rules) or when explicitly provided by
        // an author (via posInSet), do we include them in the output.
        enter: `$nameFromNode $role $state $restriction $description
          $if($posInSet, @describe_index($posInSet, $setSize))`,
        speak: `$state $nameOrTextContent= $role
          $if($posInSet, @describe_index($posInSet, $setSize))
          $description $restriction`,
      },
      [AbstractRole.LIST]: {
        startOf: `$nameFromNode $role $if($setSize, @@list_with_items($setSize))
          $restriction $description`,
        endOf: `@end_of_container($role) @@list_nested_level($listNestedLevel)`,
      },
      [AbstractRole.NAME_FROM_CONTENTS]: {
        speak: `$nameOrDescendants $node(activeDescendant) $value $state
          $restriction $role $description`,
      },
      [AbstractRole.RANGE]: {
        speak: `$name $node(activeDescendant) $description $role
          $if($value, $value, $if($valueForRange, $valueForRange))
          $state $restriction
          $if($minValueForRange, @aria_value_min($minValueForRange))
          $if($maxValueForRange, @aria_value_max($maxValueForRange))`,
      },
      [AbstractRole.SPAN]: {
        startOf: `$nameFromNode $role $state $description`,
        endOf: `@end_of_container($role)`,
      },
      [RoleType.ALERT]: {
        enter: `$name $role $state`,
        speak: `$earcon(ALERT_NONMODAL) $role $nameOrTextContent $description
          $state`,
      },
      [RoleType.ALERT_DIALOG]: {
        enter: `$earcon(ALERT_MODAL) $name $state $description $roleDescription
          $textContent`,
        speak:
            `$earcon(ALERT_MODAL) $name $nameOrTextContent $description $state
          $role`,
      },
      [RoleType.BUTTON]: {
        speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
      },
      [RoleType.CELL]: {
        enter: {
          speak: `$cellIndexText $node(tableCellColumnHeaders) $nameFromNode
            $roleDescription $state`,
          braille: `$state $cellIndexText $node(tableCellColumnHeaders)
            $nameFromNode $roleDescription`,
        },
        speak: `$nameFromNode $descendants $cellIndexText
          $node(tableCellColumnHeaders) $roleDescription $state $description`,
        braille: `$state
          $name $cellIndexText $node(tableCellColumnHeaders) $roleDescription
          $description`,
      },
      [RoleType.CHECK_BOX]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $if($checkedStateDescription, $checkedStateDescription, $checked)
          $description $state $restriction`,
      },
      [RoleType.CLIENT]: {speak: `$name`},
      [RoleType.COMBO_BOX_MENU_BUTTON]: {
        speak: `$name $value $role @aria_has_popup
          $if($setSize, @@list_with_items($setSize))
          $state $restriction $description`,
      },
      [RoleType.DATE]:
          {enter: `$nameFromNode $role $state $restriction $description`},
      [RoleType.DIALOG]: {enter: `$nameFromNode $role $description`},
      [RoleType.GENERIC_CONTAINER]: {
        enter: `$nameFromNode $description $state`,
        speak: `$nameOrTextContent $description $state`,
      },
      [RoleType.EMBEDDED_OBJECT]: {speak: `$name`},
      [RoleType.GRID]: {
        speak: `$name $node(activeDescendant) $role $state $restriction
          $description`,
      },
      [RoleType.GRID_CELL]: {
        enter: {
          speak: `$cellIndexText $node(tableCellColumnHeaders) $nameFromNode
            $roleDescription $state`,
          braille: `$state $cellIndexText $node(tableCellColumnHeaders)
            $nameFromNode $roleDescription`,
        },
        speak: `$nameFromNode $descendants $cellIndexText
          $node(tableCellColumnHeaders) $roleDescription $state $description`,
        braille: `$state
          $name $cellIndexText $node(tableCellColumnHeaders) $roleDescription
          $description
          $if($selected, @aria_selected_true)`,
      },
      [RoleType.GROUP]: {
        enter:
            `$nameFromNode $roleDescription $state $restriction $description`,
        speak: `$nameOrDescendants $value $state $restriction $roleDescription
          $description`,
        leave: ``,
      },
      [RoleType.HEADING]: {
        enter: `!relativePitch(hierarchicalLevel)
          $nameFromNode=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $description`,
        speak: `!relativePitch(hierarchicalLevel)
          $nameOrDescendants=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $restriction $description`,
      },
      [RoleType.IMAGE]: {
        speak: `$if($name, $name,
          $if($imageAnnotation, $imageAnnotation, $urlFilename))
          $value $state $role $description`,
      },
      [RoleType.IME_CANDIDATE]: {
        speak: `$name $phoneticReading @describe_index($posInSet, $setSize)`
      },
      [RoleType.INLINE_TEXT_BOX]: {speak: `$precedingBullet $name=`},
      [RoleType.INPUT_TIME]:
          {enter: `$nameFromNode $role $state $restriction $description`},
      [RoleType.LABEL_TEXT]: {
        speak: `$name $value $state $restriction $roleDescription $description`,
      },
      [RoleType.LINE_BREAK]: {speak: `$name=`},
      [RoleType.LINK]: {
        enter: `$nameFromNode= $role $state $restriction`,
        speak: `$name $value $state $restriction
          $if($inPageLinkTarget, @internal_link, $role) $description`,
      },
      [RoleType.LIST]: {
        speak: `$nameFromNode $descendants $role
          @@list_with_items($setSize) $description $state`,
      },
      [RoleType.LIST_BOX]: {
        enter: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`,
      },
      [RoleType.LIST_BOX_OPTION]: {
        speak: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $nif($selected, @aria_selected_false)`,
        braille: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $if($selected, @aria_selected_true, @aria_selected_false)`,
      },
      [RoleType.LIST_MARKER]: {speak: `$name`},
      [RoleType.MENU]: {
        enter: `$name $role `,
        speak: `$name $node(activeDescendant)
          $role @@list_with_items($setSize) $description $state $restriction`,
      },
      [RoleType.MENU_ITEM]: {
        speak: `$name $role $if($hasPopup, @has_submenu)
          @describe_index($posInSet, $setSize) $description $state $restriction`,
      },
      [RoleType.MENU_ITEM_CHECK_BOX]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $checked $state $restriction $description
          @describe_index($posInSet, $setSize)`,
      },
      [RoleType.MENU_ITEM_RADIO]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_menu_item_radio_selected($name),
          @describe_menu_item_radio_unselected($name)) $state $roleDescription
          $restriction $description
          @describe_index($posInSet, $setSize)`,
      },
      [RoleType.MENU_LIST_OPTION]: {
        speak: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $nif($selected, @aria_selected_false)`,
        braille: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $if($selected, @aria_selected_true, @aria_selected_false)`,
      },
      [RoleType.PARAGRAPH]: {speak: `$nameOrDescendants $roleDescription`},
      [RoleType.RADIO_BUTTON]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_radio_selected($name),
          @describe_radio_unselected($name))
          @describe_index($posInSet, $setSize)
          $roleDescription $description $state $restriction`,
      },
      [RoleType.ROOT_WEB_AREA]:
          {enter: `$name`, speak: `$if($name, $name, @web_content)`},
      [RoleType.REGION]:
          {speak: `$state $nameOrTextContent $description $roleDescription`},
      [RoleType.ROW]: {
        startOf: `$node(tableRowHeader) $roleDescription
          $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))`,
        speak: ` $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))
          $name $node(activeDescendant) $value $state $restriction $role
          $if($selected, @aria_selected_true) $description`,
      },
      [RoleType.STATIC_TEXT]: {speak: `$precedingBullet $name= $description`},
      [RoleType.SWITCH]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_switch_on($name),
          @describe_switch_off($name)) $roleDescription
          $description $state $restriction`,
      },
      [RoleType.TAB]: {
        speak: `@describe_tab($name) $roleDescription $description
          @describe_index($posInSet, $setSize) $state $restriction
          $if($selected, @aria_selected_true)`,
      },
      [RoleType.TABLE]: {
        enter: `$roleDescription @table_summary($name,
          $if($ariaRowCount, $ariaRowCount, $tableRowCount),
          $if($ariaColumnCount, $ariaColumnCount, $tableColumnCount))
          $node(tableHeader)`,
      },
      [RoleType.TAB_LIST]: {
        speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
      },
      [RoleType.TEXT_FIELD]: {
        speak: `$name $value
          $if($roleDescription, $roleDescription,
              $if($multiline, @tag_textarea,
                  $if($inputType, $inputType, $role)))
          $description $state $restriction`,
      },
      [RoleType.TIMER]: {
        speak: `$nameFromNode $descendants $value $state $role
        $description`,
      },
      [RoleType.TOGGLE_BUTTON]: {
        speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $pressed $description $state $restriction`,
      },
      [RoleType.TOOLBAR]: {enter: `$name $role $description $restriction`},
      [RoleType.TREE]:
          {enter: `$name $role @@list_with_items($setSize) $restriction`},
      [RoleType.TREE_ITEM]: {
        enter: `$role $expanded $collapsed $restriction
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
        speak: `$name
          $role $description $state $restriction
          $nif($selected, @aria_selected_false)
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
      },
      [RoleType.UNKNOWN]: {speak: ``},
      [RoleType.WINDOW]: {
        enter: `@describe_window($name) $description`,
        speak: `@describe_window($name) $description $earcon(OBJECT_OPEN)`,
      },
    },
    [EventType.CONTROLS_CHANGED]: {
      [RoleType.TAB]: {
        speak: `@describe_tab($name) @describe_index($posInSet, $setSize)
          @aria_selected_true`,
      },
    },
    [EventType.MENU_START]: {
      [CustomRole.DEFAULT]:
          {speak: `@chrome_menu_opened($name)  $earcon(OBJECT_OPEN)`},
    },
    [EventType.MENU_END]: {
      [CustomRole.DEFAULT]:
          {speak: `@chrome_menu_closed $earcon(OBJECT_CLOSE)`},
    },
    [EventType.ALERT]: {
      [CustomRole.DEFAULT]:
          {speak: `$earcon(ALERT_NONMODAL) $nameOrTextContent $description`},
    },
  };
}

TestImportManager.exportForTesting(OutputRule);
